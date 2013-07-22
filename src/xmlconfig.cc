#include "defs.h"
#include "xmlconfig.h"
#include <stdio.h>
#include <stdlib.h>

void set_attribute_uint(xmlpp::Element* elem,const std::string& name,unsigned int value)
{
  char ctmp[1024];
  sprintf(ctmp,"%d",value);
  elem->set_attribute(name,ctmp);
}

void set_attribute_bool(xmlpp::Element* elem,const std::string& name,bool value)
{
  if( value )
    elem->set_attribute(name,"true");
  else
    elem->set_attribute(name,"false");
}

void set_attribute_double(xmlpp::Element* elem,const std::string& name,double value)
{
  char ctmp[1024];
  sprintf(ctmp,"%1.12g",value);
  elem->set_attribute(name,ctmp);
}

void get_attribute_value(xmlpp::Element* elem,const std::string& name,double& value)
{
  std::string attv(elem->get_attribute_value(name));
  char* c;
  double tmpv(strtod(attv.c_str(),&c));
  if( c != attv.c_str() )
    value = tmpv;
}

void get_attribute_value_deg(xmlpp::Element* elem,const std::string& name,double& value)
{
  std::string attv(elem->get_attribute_value(name));
  char* c;
  double tmpv(strtod(attv.c_str(),&c));
  if( c != attv.c_str() )
    value = DEG2RAD*tmpv;
}

void get_attribute_value(xmlpp::Element* elem,const std::string& name,unsigned int& value)
{
  std::string attv(elem->get_attribute_value(name));
  char* c;
  double tmpv(strtod(attv.c_str(),&c));
  if( c != attv.c_str() )
    value = tmpv;
}


void get_attribute_value_bool(xmlpp::Element* elem,const std::string& name,bool& value)
{
  std::string attv(elem->get_attribute_value(name));
  value = (attv == "true");
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
