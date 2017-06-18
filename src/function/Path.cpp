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
#include "Function.h"
#include "ActionRegister.h"
#include "tools/OpenMP.h"

using namespace std;

namespace PLMD {
namespace function {

//+PLUMEDOC FUNCTION PATH
/*
Calculate path collective variable given a set of distances from a collection of waymarkers.

This function calculates the Path Collective Variabels that were introduced in \cite brand07.
These variables calculate the system's progress along a curvilinear path ("s" component) and the
perpendicular distance from the curvilinear path ("z" component).  

\par Examples

*/
//+ENDPLUMEDOC


class Path :
  public Function
{
private:
  double lambda;
public:
  static void registerKeywords(Keywords& keys);
  explicit Path(const ActionOptions&);
  void     calculateFunction( const std::vector<double>& args, MultiValue& myvals ) const ;
  void transformFinalValueAndDerivatives();
};


PLUMED_REGISTER_ACTION(Path,"PATH")

void Path::registerKeywords(Keywords& keys) {
  Function::registerKeywords(keys); keys.use("ARG"); keys.remove("PERIODIC");
  keys.add("compulsory","LAMBDA","the lambda parameter is needed for smoothing, is in the units of plumed");
  componentsAreNotOptional(keys);
  keys.addOutputComponent("s","default","the position on the path");
  keys.addOutputComponent("z","default","the distance from the path");
}

Path::Path(const ActionOptions&ao):
  Action(ao),
  Function(ao)
{
  rankOneOutput = getPntrToArgument(0)->getRank()>0;
  if( getPntrToArgument(0)->getRank()>1 ) error("input arguments should be rank 0 or rank 1");
  if( rankOneOutput && getNumberOfArguments()>1 ) error("cannot sum more than one vector or matrix at a time");
  if( arg_ends[1]-arg_ends[0]!=1 ) error("makes no sense to use ARG1, ARG2... with this action use single ARG keyword");
  for(unsigned i=0;i<getNumberOfArguments();++i){
     if( getPntrToArgument(i)->isPeriodic() ) error("cannot use this function on periodic functions");
  }
  parse("LAMBDA",lambda); checkRead();
  log.printf("  lambda is %f\n",lambda);
  addComponentWithDerivatives("s"); componentIsNotPeriodic("s");
  addComponentWithDerivatives("z"); componentIsNotPeriodic("z");
}

void Path::calculateFunction( const std::vector<double>& args, MultiValue& myvals ) const 
{
  if( args.size()==1 ){
      plumed_dbg_assert( done_over_stream );
      double val=exp(-lambda*args[0]); double fram = myvals.getTaskIndex() + 1;
      // Numerator
      setValue( 0, fram*val, myvals ); addDerivative( 0, 0, -lambda*fram*val, myvals );
      // Weight
      setValue( 1, val, myvals ); addDerivative( 1, 0, -lambda*val, myvals );
  } else {
      double s=0, norm=0; std::vector<double> normd( args.size() );
      for(unsigned i=0;i<args.size();++i){
          double val = exp(-lambda*args[i]); s += (i+1)*val; norm += val; normd[i] = -lambda*val;
      }
      setValue( 0, s / norm, myvals ); setValue( 1, -std::log( norm )/lambda, myvals );
      if( !doNotCalculateDerivatives() ){
          double zpref = ( 1.0/(norm*lambda) ), ddenom = s /(norm*norm);
          for(unsigned i=0;i<args.size();++i){
              // Derivatives of spath
              addDerivative( 0, i, (i+1)*normd[i]/norm - ddenom*normd[i], myvals ); 
              // Derivatives of zpath
              addDerivative( 1, i, -zpref*normd[i], myvals );
          }
      }
  }
}

void Path::transformFinalValueAndDerivatives() {
  if( !done_over_stream || getNumberOfArguments()>1 ) return;
  Value* val0 = getPntrToComponent(0); Value* val1 = getPntrToComponent(1);
  double num = val0->get(), denom = val1->get();
  val0->set( num / denom ); val1->set( -std::log( denom ) / lambda );
  if( !doNotCalculateDerivatives() ){
      double denom2 = denom*denom, zpref = 1.0 / (denom*lambda);
      for(unsigned j=0;j<val0->getNumberOfDerivatives();++j){  
          double denom_deriv = val1->getDerivative(j);
          val0->setDerivative( j, val0->getDerivative(j)/denom - denom_deriv*num/denom2 );
          val1->setDerivative( j, -zpref*denom_deriv );
      }
  }
}

}
}

