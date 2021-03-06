//-----------------------------------------------------------------------bl-
//--------------------------------------------------------------------------
//
// Antioch - A Gas Dynamics Thermochemistry Library
//
// Copyright (C) 2014-2016 Paul T. Bauman, Benjamin S. Kirk,
//                         Sylvain Plessis, Roy H. Stonger
//
// Copyright (C) 2013 The PECOS Development Team
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the Version 2.1 GNU Lesser General
// Public License as published by the Free Software Foundation.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc. 51 Franklin Street, Fifth Floor,
// Boston, MA  02110-1301  USA
//
//-----------------------------------------------------------------------el-

// This class
#include "antioch/xml_parser.h"

// Antioch
#include "antioch/chemical_mixture.h"
#include "antioch/antioch_numeric_type_instantiate_macro.h"
#include "antioch/xml_parser_instantiation_macro.h"
#include "antioch/nasa_mixture.h"
#include "antioch/cea_curve_fit.h"
#include "antioch/nasa7_curve_fit.h"

//XML
#include "antioch/tinyxml2_imp.h"

// C++
#include <sstream>
#include <limits>

namespace Antioch
{
  template <typename NumericType>
  XMLParser<NumericType>::XMLParser(const std::string &filename, bool verbose):
    ParserBase<NumericType>("XML",filename,verbose),
    _species_block(NULL),
    _reaction_block(NULL),
    _reaction(NULL),
    _rate_constant(NULL),
    _Troe(NULL)
  {
    _doc = new tinyxml2::XMLDocument;
    if(_doc->LoadFile(filename.c_str()))
      {
        std::cerr << "ERROR: unable to load xml file " << filename << std::endl;
        std::cerr << "Error of tinyxml2 library:\n"
                  << "\tID = "            << _doc->ErrorID() << "\n"
                  << "\tError String1 = " << _doc->GetErrorStr1() << "\n"
                  << "\tError String2 = " << _doc->GetErrorStr2() << std::endl;
        antioch_error();
      }

    if(this->verbose())std::cout << "Having opened file " << filename << std::endl;


    // XML block/section names
    _map[ParsingKey::PHASE_BLOCK]           = "phase";
    _map[ParsingKey::SPECIES_SET]           = "speciesArray";
    _map[ParsingKey::SPECIES_DATA]          = "speciesData";
    _map[ParsingKey::SPECIES]               = "species";
    _map[ParsingKey::THERMO]                = "thermo"; // thermo in ,<speciesData> <species> <thermo> <NASA> <floatArray> </floatArray></NASA> </thermo> </species> </speciesData>

    // Thermo parameters
    _map[ParsingKey::TMIN]                  = "Tmin";
    _map[ParsingKey::TMAX]                  = "Tmax";
    _map[ParsingKey::NASADATA]              = "floatArray";
    _map[ParsingKey::NASA7]                 = "NASA";
    _map[ParsingKey::NASA9]                 = "NASA9";

    // Kinetics parameters
    _map[ParsingKey::REACTION_DATA]         = "reactionData";
    _map[ParsingKey::REACTION]              = "reaction";
    _map[ParsingKey::REVERSIBLE]            = "reversible";
    _map[ParsingKey::ID]                    = "id";
    _map[ParsingKey::EQUATION]              = "equation";
    _map[ParsingKey::CHEMICAL_PROCESS]      = "type";
    _map[ParsingKey::KINETICS_MODEL]        = "rateCoeff";
    _map[ParsingKey::REACTANTS]             = "reactants";
    _map[ParsingKey::PRODUCTS]              = "products";
    _map[ParsingKey::FORWARD_ORDER]         = "ford";
    _map[ParsingKey::BACKWARD_ORDER]        = "rord";
    _map[ParsingKey::PREEXP]                = "A";
    _map[ParsingKey::POWER]                 = "b";
    _map[ParsingKey::ACTIVATION_ENERGY]     = "E";
    _map[ParsingKey::BERTHELOT_COEFFICIENT] = "D";
    _map[ParsingKey::TREF]                  = "Tref";
    _map[ParsingKey::HV_LAMBDA]             = "lambda";
    _map[ParsingKey::HV_CROSS_SECTION]      = "cross_section";
    _map[ParsingKey::UNIT]                  = "units";
    _map[ParsingKey::EFFICIENCY]            = "efficiencies";
    _map[ParsingKey::FALLOFF_LOW]           = "k0";
    _map[ParsingKey::FALLOFF_LOW_NAME]      = "name";
    _map[ParsingKey::TROE_FALLOFF]          = "Troe";
    _map[ParsingKey::TROE_F_ALPHA]          = "alpha";
    _map[ParsingKey::TROE_F_TS]             = "T1";
    _map[ParsingKey::TROE_F_TSS]            = "T2";
    _map[ParsingKey::TROE_F_TSSS]           = "T3";

    // typically Cantera files list
    //      pre-exponential parameters in (m3/kmol)^(m-1)/s
    //      activation energy in cal/mol, but we want it in K.
    //      power parameter without unit
    // if falloff, we need to know who's k0 and kinfty
    // if photochemistry, we have a cross-section on a lambda grid
    //      cross-section typically in cm2/nm (cross-section on a resolution bin,
    //                                          if bin unit not given, it is lambda unit (supposed to anyway), and a warning message)
    //      lambda typically in nm, sometimes in ang, default considered here is nm
    //                         you can also have cm-1, conversion is done with
    //                         formulae nm = cm-1 * / * adapted factor
    _default_unit[ParsingKey::PREEXP]                = "m3/kmol";
    _default_unit[ParsingKey::POWER]                 = "";
    _default_unit[ParsingKey::ACTIVATION_ENERGY]    = "cal/mol";
    _default_unit[ParsingKey::BERTHELOT_COEFFICIENT] = "K-1";
    _default_unit[ParsingKey::TREF]                  = "K";
    _default_unit[ParsingKey::HV_LAMBDA]             = "nm";
    _default_unit[ParsingKey::HV_CROSS_SECTION]      = "cm2/nm";
    _default_unit[ParsingKey::EFFICIENCY]            = "";
    _default_unit[ParsingKey::TROE_F_ALPHA]          = "";
    _default_unit[ParsingKey::TROE_F_TS]             = "K";
    _default_unit[ParsingKey::TROE_F_TSS]            = "K";
    _default_unit[ParsingKey::TROE_F_TSSS]           = "K";

    //gri30
     _gri_map[GRI30Comp::FALLOFF]      = "falloff";
     _gri_map[GRI30Comp::FALLOFF_TYPE] = "type";
     _gri_map[GRI30Comp::TROE]         = "Troe";

    this->initialize();
  }

  template <typename NumericType>
  XMLParser<NumericType>::~XMLParser()
  {
     delete _doc;
  }

  template <typename NumericType>
  void XMLParser<NumericType>::change_file(const std::string & filename)
  {
    ParserBase<NumericType>::_file = filename;
    _species_block  = NULL;
    _reaction_block = NULL;
    _reaction       = NULL;
    _rate_constant  = NULL;
    _Troe           = NULL;

    delete _doc;
    _doc = new tinyxml2::XMLDocument;
    if(_doc->LoadFile(filename.c_str()))
      {
        std::cerr << "ERROR: unable to load xml file " << filename << std::endl;
        std::cerr << "Error of tinyxml2 library:\n"
                  << "\tID = "            << _doc->ErrorID() << "\n"
                  << "\tError String1 = " << _doc->GetErrorStr1() << "\n"
                  << "\tError String2 = " << _doc->GetErrorStr2() << std::endl;
        antioch_error();
      }

    if(this->verbose())std::cout << "Having opened file " << filename << std::endl;

    this->initialize();
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::initialize()
  {
    //we start here
    _reaction_block = _doc->FirstChildElement("ctml");
    if (!_reaction_block)
      {
        std::cerr << "ERROR:  no <ctml> tag found in input file"
                  << std::endl;
        antioch_error();
      }

    _species_block = _reaction_block->FirstChildElement(_map.at(ParsingKey::PHASE_BLOCK).c_str());
    if(_species_block)
        _species_block = _species_block->FirstChildElement(_map.at(ParsingKey::SPECIES_SET).c_str());

    _thermo_block = _reaction_block->FirstChildElement(_map.at(ParsingKey::SPECIES_DATA).c_str());

    _reaction_block = _reaction_block->FirstChildElement(_map.at(ParsingKey::REACTION_DATA).c_str());

    _reaction = NULL;
    _rate_constant = NULL;

    return _reaction_block;
  }



  template <typename NumericType>
  const std::vector<std::string> XMLParser<NumericType>::species_list()
  {
    if(!_species_block)
      antioch_error_msg("ERROR: Could not find "+_map.at(ParsingKey::SPECIES_SET)+" section in input file!");

    std::vector<std::string> molecules;

    split_string(std::string(_species_block->GetText())," ",molecules);
    remove_newline_from_strings(molecules);

    return molecules;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::reaction()
  {
    antioch_assert(_reaction_block);
    _reaction = (_reaction)?
      _reaction->NextSiblingElement(_map.at(ParsingKey::REACTION).c_str()):
      _reaction_block->FirstChildElement(_map.at(ParsingKey::REACTION).c_str());

    _rate_constant = NULL;
    _Troe          = NULL;

    return _reaction;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant(const std::string & kinetics_model)
  {
    // if in a reaction
    if(_reaction)
      {
        // not the first one
        if(_rate_constant)
          {
            _rate_constant = _rate_constant->NextSiblingElement(kinetics_model.c_str());
          }else
          {
            // first one, we need to set _rate_constant and _Troe, because they contain environments
            // we suppose that there is a rateCoeff environement
            // _rate_constant => <rateCoeff> <kin model> </kin model> </rateCoeff>
            // _Troe          => <rateCoeff> <Troe> </Troe> </rateCoeff> || <rateCoeff><falloff type="Troe"></falloff></rateCoef>
            antioch_assert(_reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str()));
            _rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(kinetics_model.c_str());
            _Troe          = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(_map.at(ParsingKey::TROE_FALLOFF).c_str());
            if(_Troe == NULL)
            {
              _Troe = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(_gri_map.at(GRI30Comp::FALLOFF).c_str());
              if(_Troe)
              {
                if(std::string(_Troe->Attribute(_gri_map.at(GRI30Comp::FALLOFF_TYPE).c_str())).compare(_gri_map.at(GRI30Comp::TROE)) != 0)
                {
                  _Troe = NULL;
                }
              }
            }
          }
      }else
      {
        _rate_constant = NULL;
      }

    return _rate_constant;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe() const
  {
    return _Troe;
  }

  template <typename NumericType>
  const std::string XMLParser<NumericType>::reaction_id() const
  {
    std::stringstream id;
    id << _reaction->Attribute(_map.at(ParsingKey::ID).c_str());
    return id.str();
  }

  template <typename NumericType>
  const std::string XMLParser<NumericType>::reaction_equation() const
  {
    return _reaction->FirstChildElement(_map.at(ParsingKey::EQUATION).c_str())->GetText();
  }

  template <typename NumericType>
  const std::string XMLParser<NumericType>::reaction_chemical_process() const
  {
    const char * chem_proc = _reaction->Attribute(_map.at(ParsingKey::CHEMICAL_PROCESS).c_str());

    if(chem_proc)
    {
       // we're GRI
     if(std::string(chem_proc).compare(_gri_map.at(GRI30Comp::FALLOFF)) == 0)
     {
       // find the falloff block
       // are we threebody?
       // if equation contains (+M)
       const std::string & eq = this->reaction_equation();
       // <reaction><rateCoeff><falloff type=''/></rateCoeff></reaction>
       std::string Lind = "LindemannFalloff";
       std::string Troe = "TroeFalloff";

       // Travis CI gives us regex errors so we'll fall back on this:
       const char * searchfor = "(+M)";
       for (std::size_t pos = eq.find('('); pos < eq.size() && *searchfor; ++pos)
         {
           if (eq[pos] == *searchfor)
             {
               ++searchfor;
               continue;
             }

           if (eq[pos] != ' ')
             break;
         }

       // If we found everything then *searchfor is NULL
       if (!*searchfor)
       {
          Lind += "ThreeBody";
          Troe += "ThreeBody";
       }
       tinyxml2::XMLElement * fall = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(_gri_map.at(GRI30Comp::FALLOFF).c_str());
       const char * falloff = fall->Attribute(_map.at(ParsingKey::CHEMICAL_PROCESS).c_str());
       const char * cp = (Lind.find(falloff) != std::string::npos) ? Lind.c_str() :
         (Troe.find(falloff) != std::string::npos) ? Troe.c_str() : chem_proc;

        return (cp)?
          std::string(cp) :
          std::string();
     }

    }

    return (chem_proc)?
      std::string(chem_proc) :
      std::string();
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::reaction_reversible() const
  {
    return (_reaction->Attribute(_map.at(ParsingKey::REVERSIBLE).c_str()))?
      (std::string(_reaction->Attribute(_map.at(ParsingKey::REVERSIBLE).c_str())) == std::string("no"))?false:true //explicit
      :
      true; //default
  }

  template <typename NumericType>
  const std::string XMLParser<NumericType>::reaction_kinetics_model(const std::vector<std::string> &kinetics_models) const
  {
    unsigned int imod(0);
    tinyxml2::XMLElement * rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(kinetics_models[imod].c_str());
    while(!rate_constant)
      {
        if(imod == kinetics_models.size() - 1)
          {
            std::cerr << "Could not find a suitable kinetics model.\n"
                      << "Implemented kinetics models are:\n";

            for(unsigned int m = 0; m < kinetics_models.size(); m++)
              std::cerr << "  " << kinetics_models[m] << "\n";

            std::cerr << "See Antioch documentation for more details."
                      << std::endl;
            antioch_not_implemented();
          }
        imod++;
        rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str())->FirstChildElement(kinetics_models[imod].c_str());
      }

    return kinetics_models[imod];
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::reactants_pairs(std::vector<std::pair<std::string,int> > & reactants_pair) const
  {
    tinyxml2::XMLElement* reactants = _reaction->FirstChildElement(_map.at(ParsingKey::REACTANTS).c_str());
    return this->molecules_pairs(reactants, reactants_pair);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::products_pairs(std::vector<std::pair<std::string,int> > & products_pair) const
  {
    tinyxml2::XMLElement* products = _reaction->FirstChildElement(_map.at(ParsingKey::PRODUCTS).c_str());
    return this->molecules_pairs(products, products_pair);
  }

  template <typename NumericType>
  const std::map<std::string,NumericType> XMLParser<NumericType>::reactants_orders() const
  {
    tinyxml2::XMLElement* orders = _reaction->FirstChildElement(_map.at(ParsingKey::FORWARD_ORDER).c_str());
    std::map<std::string,NumericType> map;
    if(orders){
      std::vector<std::pair<std::string,NumericType> > pairs;
      if(this->molecules_pairs(orders,pairs))
      {
         for(unsigned int s = 0; s < pairs.size(); s++)
         {
            map.insert(pairs[s]);
         }
      }
    }

    return map;
  }

  template <typename NumericType>
  const std::map<std::string,NumericType> XMLParser<NumericType>::products_orders() const
  {
    tinyxml2::XMLElement* orders = _reaction->FirstChildElement(_map.at(ParsingKey::BACKWARD_ORDER).c_str());
    std::map<std::string,NumericType> map;
    if(orders){
      std::vector<std::pair<std::string,NumericType> > pairs;
      if(this->molecules_pairs(orders,pairs))
      {
         for(unsigned int s = 0; s < pairs.size(); s++)
         {
            map.insert(pairs[s]);
         }
      }
    }
    return map;
  }


  template <typename NumericType>
  template <typename PairedType>
  bool XMLParser<NumericType>::molecules_pairs(tinyxml2::XMLElement * molecules, std::vector<std::pair<std::string,PairedType> > & molecules_pairs) const
  {
    bool out(true);
    if(molecules)
      {

        std::vector<std::string> mol_pairs;

        // Split the reactant string on whitespace. If no entries were found,
        // there is no whitespace - and assume then only one reactant is listed.
        split_string(std::string(molecules->GetText()), " ", mol_pairs);

        for( unsigned int p=0; p < mol_pairs.size(); p++ )
          {
            std::pair<std::string,PairedType> pair( split_string_on_colon<PairedType>(mol_pairs[p]) );

            molecules_pairs.push_back(pair);
          }
      }else
      {
        out = false;
      }

    return out;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::is_k0(unsigned int nrc, const std::string & kin_model) const
  {
    bool k0(false);
    if(_rate_constant->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str()))
      {
        if(std::string(_rate_constant->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str())) == _map.at(ParsingKey::FALLOFF_LOW))
        {
          k0 = true;
        // now verifying the second one
          if(nrc == 0) // first reaction rate block
          {
            antioch_assert(_rate_constant->NextSiblingElement(kin_model.c_str()));
            if(_rate_constant->NextSiblingElement(kin_model.c_str())->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str())) // HAHA
            {
               std::string error = "I can understand the need to put attributes everywhere, really, but in this case, I'm ";
               error += "afraid that it's not a good idea to have two \'name\' attributes: only the low pressure limit should have it.";
               antioch_parsing_error(error);
            }
          }
        }else
        {
          std::string error = "The keyword associated with the \'name\' attribute within the description of a falloff should be, and only be, ";
          error += "\'k0\' to specify the low pressure limit.  It seems that the one you provided, \'";
          error += std::string(_rate_constant->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str()));
          error += "\' is not this one.  Please correct it at reaction";
          error += this->reaction_id();
          error += ": ";
          error += this->reaction_equation();
          error += ".";
          antioch_parsing_error(error);
        }
      }else if(nrc == 0) // if we're indeed at the first reading
      {
        antioch_assert(_rate_constant->NextSiblingElement(kin_model.c_str()));
        if(!_rate_constant->NextSiblingElement(kin_model.c_str())->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str())) // and the next doesn't have a name
          {
            k0 = true;
          }else
          {
            if(std::string(_rate_constant->NextSiblingElement(kin_model.c_str())->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str())) == _map.at(ParsingKey::FALLOFF_LOW))k0 = false;
          }
      }
    return k0;
  }

  template <typename NumericType>
  unsigned int XMLParser<NumericType>::where_is_k0(const std::string & kin_model) const
  {
    antioch_assert(!_rate_constant); //should be done exterior to rate constant block
    antioch_assert(_reaction);       //should be done interior to reaction block

    tinyxml2::XMLElement * rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str());
    antioch_assert(rate_constant);
    rate_constant = rate_constant->FirstChildElement(kin_model.c_str());
    unsigned int k0(0);
    if(rate_constant->NextSiblingElement()->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str()))
      {
        if(std::string(rate_constant->NextSiblingElement()->Attribute(_map.at(ParsingKey::FALLOFF_LOW_NAME).c_str())) == _map.at(ParsingKey::FALLOFF_LOW))k0=1;
      }

    return k0;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::get_parameter(const tinyxml2::XMLElement * ptr, const std::string & par, NumericType & par_value, std::string & par_unit) const
  {
    antioch_assert(ptr);

    bool out(false);
    par_unit.clear();
    if(ptr->FirstChildElement(par.c_str()))
      {
        par_value = std::atof(ptr->FirstChildElement(par.c_str())->GetText());
        if(ptr->FirstChildElement(par.c_str())->Attribute(_map.at(ParsingKey::UNIT).c_str()))
          par_unit = ptr->FirstChildElement(par.c_str())->Attribute(_map.at(ParsingKey::UNIT).c_str());
        out = true;
      }

    return out;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::get_parameter(const tinyxml2::XMLElement * ptr, const std::string & par, std::vector<NumericType> & par_values, std::string & par_unit) const
  {
    antioch_assert(ptr);

    bool out(false);
    par_unit.clear();
    if(ptr->FirstChildElement(par.c_str()))
      {
        std::vector<std::string> values;
        split_string(ptr->FirstChildElement(par.c_str())->GetText()," ",values);

        par_values.resize(values.size());
        for(unsigned int i = 0; i < values.size(); i++)
          par_values[i] =  string_to_T<NumericType>(values[i].c_str());

        if(ptr->FirstChildElement(par.c_str())->Attribute(_map.at(ParsingKey::UNIT).c_str()))
          par_unit = ptr->FirstChildElement(par.c_str())->Attribute(_map.at(ParsingKey::UNIT).c_str());

        out = true;
      }

    return out;
  }

  template <typename NumericType>
  tinyxml2::XMLElement* XMLParser<NumericType>::find_element_with_attribute( const tinyxml2::XMLElement * element,
                                                                             const std::string& elem_name,
                                                                             const std::string& attribute,
                                                                             const std::string& attr_value ) const
  {
    antioch_assert(element);

    const tinyxml2::XMLElement * elem_with_attr = NULL;

    if( !element->Attribute(attribute.c_str()) )
      antioch_error_msg("ERROR: Could not find attribute "+attribute+" for current element!");

    // First check if the first element has the attribute we're looking for
    if( std::string( element->Attribute(attribute.c_str()) ) == attr_value )
      elem_with_attr = element;

    // Otherwise, look at all the siblings
    else
      {
        elem_with_attr = element->NextSiblingElement(elem_name.c_str());

        while( elem_with_attr )
          {
            std::string curr_attr;
            if( elem_with_attr->Attribute(attribute.c_str()) )
              curr_attr = std::string(elem_with_attr->Attribute(attribute.c_str()));

            if( curr_attr == attr_value )
              break;

            elem_with_attr = elem_with_attr->NextSiblingElement(elem_name.c_str());
          }

        // Error out if we couldn't find the attribute with the correct value
        if( !elem_with_attr )
          antioch_error_msg("ERROR: Could not find XMLElement with attribute = "+attribute+" whose value is "+attr_value+"!");

      }

    return const_cast<tinyxml2::XMLElement*>(elem_with_attr);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_preexponential_parameter(NumericType & A, std::string & A_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::PREEXP);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::PREEXP).c_str(),A,A_unit);
  }


  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_power_parameter(NumericType & b, std::string & b_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::POWER);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::POWER).c_str(),b,b_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_activation_energy_parameter(NumericType & Ea, std::string & Ea_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::ACTIVATION_ENERGY);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::ACTIVATION_ENERGY).c_str(),Ea,Ea_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_Berthelot_coefficient_parameter(NumericType & D, std::string & D_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::BERTHELOT_COEFFICIENT);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::BERTHELOT_COEFFICIENT).c_str(),D,D_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_lambda_parameter(std::vector<NumericType> & lambda, std::string & lambda_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::HV_LAMBDA);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::HV_LAMBDA).c_str(),lambda,lambda_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_cross_section_parameter(std::vector<NumericType> & sigma, std::string & sigma_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::HV_CROSS_SECTION);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::HV_CROSS_SECTION).c_str(),sigma,sigma_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::rate_constant_Tref_parameter(NumericType & Tref, std::string & Tref_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::TREF);
    return this->get_parameter(_rate_constant,_map.at(ParsingKey::TREF).c_str(),Tref,Tref_unit);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::verify_Kooij_in_place_of_Arrhenius() const
  {
    bool out(false);
    tinyxml2::XMLElement * rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str());
    antioch_assert(rate_constant->FirstChildElement("Arrhenius"));
    rate_constant = rate_constant->FirstChildElement("Arrhenius");
    if(rate_constant->FirstChildElement(_map.at(ParsingKey::POWER).c_str()))
      {
        if(std::atof(rate_constant->FirstChildElement(_map.at(ParsingKey::POWER).c_str())->GetText()) != 0.) //not a very good test
          out = true;
      }

    return out;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::efficiencies(std::vector<std::pair<std::string,NumericType> > & par_values) const
  {
    bool out = false;
    if(_reaction)
      {
        tinyxml2::XMLElement * rate_constant = _reaction->FirstChildElement(_map.at(ParsingKey::KINETICS_MODEL).c_str());
        if(rate_constant)
          {
            if(rate_constant->FirstChildElement(_map.at(ParsingKey::EFFICIENCY).c_str()))
              {
                std::vector<std::string> values;
                std::string value_string = std::string((rate_constant->FirstChildElement(_map.at(ParsingKey::EFFICIENCY).c_str())->GetText())?rate_constant->FirstChildElement(_map.at(ParsingKey::EFFICIENCY).c_str())->GetText():"");

                split_string(value_string, " ", values);

                for(unsigned int i = 0; i < values.size(); i++)
                    par_values.push_back(split_string_on_colon<NumericType>(values[i]));

                out = true;
              }
          }
      }
    return out;
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe_GRI_parameter(NumericType & parameter, unsigned int index) const
  {
   // Troe parameters block
   // [alpha, T***, T*, T**]
    std::string Troe_block = std::string(_Troe->GetText());
   // do we have something?
    bool gri = Troe_block.size() != 0;
    if(gri)
    {
      std::vector<std::string> values;
      split_string(Troe_block, " ", values);
      // T** is optional, we generalize ?? should we ??
      if(index < values.size())
      {
        parameter = std::atof(values[index].c_str());
      }else{
        gri = false;
      }
     }
     return gri;
   }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe_alpha_parameter(NumericType & alpha, std::string & alpha_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::TROE_F_ALPHA);
    // if defined as Antioch wants it
    bool antioch = this->get_parameter(_Troe,_map.at(ParsingKey::TROE_F_ALPHA),alpha,alpha_unit);
    // if not, we try GRI30
    return antioch ? antioch : this->Troe_GRI_parameter(alpha,0);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe_T1_parameter(NumericType & T1, std::string & T1_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::TROE_F_TS);
    // if defined as Antioch wants it
    bool antioch = this->get_parameter(_Troe,_map.at(ParsingKey::TROE_F_TS),T1,T1_unit);
    // if not, we try GRI30
    return antioch ? antioch : this->Troe_GRI_parameter(T1,2);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe_T2_parameter(NumericType & T2, std::string & T2_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::TROE_F_TSS);
    // if defined as Antioch wants it
    bool antioch = this->get_parameter(_Troe,_map.at(ParsingKey::TROE_F_TSS),T2,T2_unit);
    // if not, we try GRI30
    return antioch ? antioch : this->Troe_GRI_parameter(T2,3);
  }

  template <typename NumericType>
  bool XMLParser<NumericType>::Troe_T3_parameter(NumericType & T3, std::string & T3_unit, std::string & def_unit) const
  {
    def_unit = _default_unit.at(ParsingKey::TROE_F_TSSS);
    // if defined as Antioch wants it
    bool antioch = this->get_parameter(_Troe,_map.at(ParsingKey::TROE_F_TSSS),T3,T3_unit);
    // if not, we try GRI30
    return antioch ? antioch : this->Troe_GRI_parameter(T3,1);
  }


  template <typename NumericType>
  template <typename ThermoType>
  void XMLParser<NumericType>::read_thermodynamic_data_root(ThermoType & thermo)
  {
    if(!_thermo_block)
      antioch_error_msg("ERROR: No "+_map.at(ParsingKey::SPECIES_DATA)+" section found! Cannot parse thermo!");

    const ChemicalMixture<NumericType> & chem_mixture = thermo.chemical_mixture();
    const std::vector<ChemicalSpecies<NumericType>*>& chem_species = chem_mixture.chemical_species();

    // Based on the ThermoType, namely the CurveFit, we deduce what the section name is.
    std::string nasa_xml_section = this->nasa_xml_section(thermo);

    for(unsigned int s = 0; s < chem_mixture.n_species(); s++)
      {
        // Step to first species block
        tinyxml2::XMLElement * species_block = _thermo_block->FirstChildElement(_map.at(ParsingKey::SPECIES).c_str());

        if(!species_block)
          antioch_error_msg("ERROR: No "+_map.at(ParsingKey::SPECIES)+" block found within "+_map.at(ParsingKey::SPECIES_DATA)+" section! Cannot parse thermo!");


        const std::string& name = chem_species[s]->species();

        tinyxml2::XMLElement * spec = NULL;
        spec = this->find_element_with_attribute( species_block,
                                                  _map.at(ParsingKey::SPECIES),
                                                  "name",
                                                  name );

        if(!spec)
          antioch_error_msg("ERROR: Species "+name+" has not been found in the "+_map.at(ParsingKey::SPECIES_DATA)+" section! Cannot parse thermo!");

        else
          {
            spec = spec->FirstChildElement(_map.at(ParsingKey::THERMO).c_str());

            if(!spec)
              antioch_error_msg("ERROR: No "+_map.at(ParsingKey::THERMO)+" block found for species "+name+"! Cannot parse thermo!");

            // containers for parsing thermo data
            std::vector<NumericType> temps;
            std::vector<NumericType> values;
            tinyxml2::XMLElement * coeffs;
            std::vector<std::string> coeffs_str;

            // looping for each of the temperature intervals for this species
            tinyxml2::XMLElement * nasa = spec->FirstChildElement(nasa_xml_section.c_str());
            if(!nasa)
              antioch_error_msg("ERROR: Could not find "+nasa_xml_section+" thermo section!");

            while(nasa)
              {
                if( !(nasa->Attribute(_map.at(ParsingKey::TMIN).c_str())) )
                  antioch_error_msg("ERROR: Could not find "+_map.at(ParsingKey::TMIN)+" attribute for species "+name+"!");

                if( !(nasa->Attribute(_map.at(ParsingKey::TMAX).c_str())) )
                  antioch_error_msg("ERROR: Could not find "+_map.at(ParsingKey::TMAX)+" attribute for species "+name+"!");
                // By convention, we put the first TMIN in, and then only the TMAX thereafter
                // We have a consistency check below to make sure the TMIN's in the input are consistent
                if( temps.empty() )
                  temps.push_back(string_to_T<NumericType>(nasa->Attribute(_map.at(ParsingKey::TMIN).c_str())));

                // temperatures, only Tmax as Tmin is suppose to be last Tmax
                temps.push_back(string_to_T<NumericType>(nasa->Attribute(_map.at(ParsingKey::TMAX).c_str())));

                // now coeffs
                if( !(nasa->FirstChildElement(_map.at(ParsingKey::NASADATA).c_str())) )
                  antioch_error_msg("ERROR: Could not find "+_map.at(ParsingKey::NASADATA)+" data for species "+name+"!");

                coeffs = nasa->FirstChildElement(_map.at(ParsingKey::NASADATA).c_str());
                split_string(std::string(coeffs->GetText())," ",coeffs_str);
                remove_newline_from_strings(coeffs_str);

                for(unsigned int d = 0; d < coeffs_str.size(); d++)
                  values.push_back(string_to_T<NumericType>(coeffs_str[d]));

                // If we have more than one interval, make sure the temperature intervals match up
                if( temps.size() > 1 )
                  {
                    NumericType prev_Tmax = *(temps.end()-2);
                    NumericType Tmin = string_to_T<NumericType>(nasa->Attribute(_map.at(ParsingKey::TMIN).c_str()));
                    NumericType diff = (Tmin - prev_Tmax)/prev_Tmax;

                    const NumericType tol = std::numeric_limits<NumericType>::epsilon() * 10.;

                    if(std::abs(diff) > tol)
                      antioch_error_msg("ERROR: Tmax/Tmin mismatch for species "+name+"!");
                  }

                // This is meant to store only data one interval at a time, so we must clear at each iteration
                coeffs_str.clear();

                // Move onto next interval of data
                nasa = nasa->NextSiblingElement(nasa_xml_section.c_str());

              } // end while loop

            thermo.add_curve_fit(name, values, temps);
          }

      } // end species loop
  }

  // Instantiate
  ANTIOCH_NUMERIC_TYPE_CLASS_INSTANTIATE(XMLParser);
  ANTIOCH_XML_PARSER_INSTANTIATE();

} // end namespace Antioch
