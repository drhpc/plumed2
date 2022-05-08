/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2020 The plumed team
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
#include "CLTool.h"
#include "CLToolRegister.h"
#include "tools/Tools.h"
#include "config/Config.h"
#include "core/ActionRegister.h"
#include <cstdio>
#include <string>
#include <iostream>

namespace PLMD {
namespace cltools {

//+PLUMEDOC TOOLS gen_json
/*
gen_json constructs a json file that includes a dictionary of actions, the keywords for those actions and the components and outputs this to standard output

\par Examples

The following command generates the json file
\verbatim
plumed gen_json
\endverbatim


*/
//+ENDPLUMEDOC

class GenJson : public CLTool {
private:
  std::string version;
public:
  static void registerKeywords( Keywords& keys );
  explicit GenJson(const CLToolOptions& co );
  int main(FILE* in, FILE*out,Communicator& pc) override;
  std::string description()const override {
    return "print out a json file that contains the pluemd syntax";
  }
};

PLUMED_REGISTER_CLTOOL(GenJson,"gen_json")

void GenJson::registerKeywords( Keywords& keys ) {
  CLTool::registerKeywords( keys );
}

GenJson::GenJson(const CLToolOptions& co ):
  CLTool(co),
  version("master")
{
  inputdata=commandline;
  if( config::getVersionLong().find("dev")==std::string::npos ) version="v"+config::getVersion();
}

int GenJson::main(FILE* in, FILE*out,Communicator& pc) {

  // Get the names of all the actions 
  std::vector<std::string> action_names( actionRegister().getActionNames() );
  // Cycle over all the action names
  std::cout<<"{"<<std::endl;
  for(unsigned i=0;i<action_names.size();++i) {
    std::cout<<"  \""<<action_names[i]<<'"'<<": {"<<std::endl; std::string action=action_names[i];  
    // Handle conversion of action names to links
    std::cout<<"    \"hyperlink\" : \"https://www.plumed.org/doc-"<<version<<"/user-doc/html/";
    std::transform(action.begin(), action.end(), action.begin(), [](unsigned char c) { return std::tolower(c); });
    for(unsigned n=0;; ++n) {
      std::size_t und=action.find_first_of("_");
      if( und==std::string::npos ) break;
      std::string first=action.substr(0,und);
      for(auto c : first ) { if( isdigit(c) ) std::cout<<c; else std::cout<<"_"<<c; }
      std::cout<<"_"; action=action.substr(und+1);
    }
    for(auto c : action ) { if( isdigit(c) ) std::cout<<c; else std::cout<<"_"<<c; }
    std::cout<<".html\","<<std::endl;
    // Now output keyword information
    Keywords keys; actionRegister().getKeywords( action_names[i], keys );
    for(unsigned j=0;j<keys.size();++j){
        std::string desc = keys.getKeywordDescription( keys.getKeyword(j) ); 
        if( desc.find("default=")!=std::string::npos ) { 
             std::size_t brac=desc.find_first_of(")"); desc = desc.substr(brac+1); 
        } 
        std::size_t dot=desc.find_first_of(".");
        if( j==keys.size()-1 && !keys.exists("HAS_VALUES") ) std::cout<<"    \""<<keys.getKeyword(j)<<"\" : \""<<desc.substr(0,dot)<<"\""<<std::endl;
        else std::cout<<"    \""<<keys.getKeyword(j)<<"\" : \""<<desc.substr(0,dot)<<"\","<<std::endl;
    }
    if( keys.exists("HAS_VALUES") ) {
        std::cout<<"    \"output\" : {"<<std::endl;
        std::vector<std::string> components( keys.getOutputComponents() );
        // Check if we have a value
        bool hasvalue=true;
        for(unsigned k=0; k<components.size(); ++k) {
            if( keys.getOutputComponentFlag( components[k] )=="default" ) { hasvalue=false; break; }
        } 
        if( hasvalue ) {
            std::cout<<"      \"value\": {"<<std::endl;
            std::cout<<"        \"flag\": \"value\","<<std::endl;
            std::cout<<"        \"description\": \"a scalar quantity\""<<std::endl;
            if( components.size()==0 ) std::cout<<"      }"<<std::endl; else std::cout<<"      },"<<std::endl;
        }
        for(unsigned k=0; k<components.size(); ++k) {
            std::cout<<"      \""<<components[k]<<"\" : {"<<std::endl;
            std::cout<<"        \"flag\": \""<<keys.getOutputComponentFlag( components[k] )<<"\","<<std::endl;
            std::string desc=keys.getOutputComponentDescription( components[k] ); std::size_t dot=desc.find_first_of(".");
            std::cout<<"        \"description\": \""<<desc.substr(0,dot)<<"\""<<std::endl;
            if( k==components.size()-1 ) std::cout<<"      }"<<std::endl; else std::cout<<"      },"<<std::endl;  
        }
        std::cout<<"    }"<<std::endl;

    } 
    if(i==action_names.size()-1) std::cout<<"  }"<<std::endl; 
    else std::cout<<"  },"<<std::endl;
  }
  std::cout<<"}"<<std::endl;
  return 0;
}

} // End of namespace
}