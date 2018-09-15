/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2016,2017 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "PathProjectionCalculator.h"
#include "core/ActionSet.h"
#include "core/ActionRegister.h"

namespace PLMD {
namespace mapping {

void PathProjectionCalculator::registerKeywords(Keywords& keys) {
  keys.use("ARG");
  keys.add("compulsory","METRIC","the method to use for computing the displacement vectors between the reference frames");
  keys.add("compulsory","REFFRAMES","labels for actions that contain reference coordinates for each point on the path");
}

PathProjectionCalculator::PathProjectionCalculator( ActionWithArguments* act ):
mypath_obj(act->getPntrToArgument(0))
{
  // Ensure that values are stored in base calculation and that PLUMED doesn't try to calculate this in the stream
  mypath_obj->buildDataStore( act->getLabel() );
  // Check that we have only one argument as input
  if( act->getNumberOfArguments()!=1 ) act->error("should only have one argument to this function");
  // Check that the input is a matrix
  if( mypath_obj->getRank()!=2 ) act->error("the input to this action should be a matrix");
  // Get the labels for the reference points
  unsigned natoms, nargs; std::vector<std::string> reflabs( mypath_obj->getShape()[0] ); act->parseVector("REFFRAMES", reflabs );
  for(unsigned i=0;i<reflabs.size();++i) {
      setup::SetupReferenceBase* rv = act->plumed.getActionSet().selectWithLabel<setup::SetupReferenceBase*>( reflabs[i] );
      if( !rv ) act->error("input " + reflabs[i] + " is not a READ_CONFIG action");
      reference_frames.push_back( rv ); unsigned tatoms, targs; rv->getNatomsAndNargs( tatoms, targs );
      if( i==0 ) { natoms=tatoms; nargs=targs; }
      else if( natoms!=tatoms || nargs!=targs ) act->error("mismatched reference configurations");
  }
  // Create a plumed main object to compute distances between reference configurations
  int s=sizeof(double);
  metric.cmd("setRealPrecision",&s);
  metric.cmd("setNoVirial");
  metric.cmd("setMDEngine","plumed");
  int nat=2*natoms; metric.cmd("setNatoms",&nat);
  positions.resize(nat); masses.resize(nat); forces.resize(nat); charges.resize(nat);
  if( nargs>0 ) {
      std::vector<int> size(2); size[0]=1; size[1]=nargs;
      metric.cmd("createValue arg1",&size[0]);
      metric.cmd("createValue arg2",&size[0]);
      if( !mypath_obj->isPeriodic() ) {
          metric.cmd("setValueNotPeriodic arg1"); metric.cmd("setValueNotPeriodic arg2");
      } else {
          std::string min, max; mypath_obj->getDomain( min, max );
          std::string dom( min + " " + max ); unsigned doml = dom.length();
          char domain[doml+1]; strcpy( domain, dom.c_str());
          metric.cmd("setValueDomain arg1", domain );
          metric.cmd("setValueDomain arg2", domain );
      }
  }
  double tstep=1.0; metric.cmd("setTimestep",&tstep);
  std::string inp; act->parse("METRIC",inp); const char* cinp=inp.c_str();
  std::vector<std::string> input=Tools::getWords(inp);
  if( input.size()==1 && !actionRegister().check(input[0]) ) {
      metric.cmd("setPlumedDat",cinp); metric.cmd("init");
  } else {
      metric.cmd("init"); metric.cmd("readInputLine",cinp);
  }
  // Now setup stuff to retrieve the final displacement
  ActionWithValue* fav = dynamic_cast<ActionWithValue*>( metric.getActionSet()[metric.getActionSet().size()-1].get() );
  if( !fav ) act->error("final value should calculate relevant value that you want as reference");
  std::string name = (fav->copyOutput(0))->getName(); long rank; metric.cmd("getDataRank " + name, &rank );
  if( rank==0 ) rank=1;
  std::vector<long> ishape( rank ); metric.cmd("getDataShape " + name, &ishape[0] );
  unsigned nvals=1; for(unsigned i=0;i<ishape.size();++i) nvals *= ishape[i];
  data.resize( nvals ); metric.cmd("setMemoryForData " + name, &data[0] );

}

double PathProjectionCalculator::getProjectionOnPath( const unsigned& ifrom, const unsigned& ito, const unsigned& closest, const Tensor& box, double& len ) {
  int step = 1; metric.cmd("setStep",&step);
  reference_frames[ifrom]->transferDataToPlumed( 0, masses, charges, positions, "arg1", metric );
  reference_frames[ito]->transferDataToPlumed( positions.size()/2, masses, charges, positions, "arg2", metric );
  metric.cmd("setMasses",&masses[0]); metric.cmd("setCharges",&charges[0]);
  metric.cmd("setPositions",&positions[0]); metric.cmd("setForces",&forces[0]);
  metric.cmd("setBox",&box[0][0]); metric.cmd("calc");
  double fval=0; len=0; unsigned k=mypath_obj->getShape()[1]*closest;
  for(unsigned i=0;i<data.size();++i) { len += data[i]*data[i]; fval += data[i]*mypath_obj->get(k+i); }
  return fval;
}

std::string PathProjectionCalculator::getReferenceLabel( const unsigned& iframe ) const {
  return reference_frames[iframe]->getLabel();
}

}
}
