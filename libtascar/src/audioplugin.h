/**
 * @file   audioplugin.h
 * @author Giso Grimm
 * 
 * @brief  Base class for audio signal processing plugins
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef AUDIOPLUGIN_H
#define AUDIOPLUGIN_H

#include "xmlconfig.h"
//#include "audiochunks.h"
#include "tascarplugin.h"
#include "licensehandler.h"

namespace TASCAR {

  class audioplugin_cfg_t {
  public:
    audioplugin_cfg_t(xmlpp::Element* xmlsrc, const std::string& name, const std::string& parentname):xmlsrc(xmlsrc),name(name),parentname(parentname){};
    xmlpp::Element* xmlsrc;
    const std::string& name;
    const std::string& parentname;
    std::string modname;
  };

  /**
     \brief Base class of audio processing plugins
  */
  class audioplugin_base_t : public xml_element_t, public audiostates_t {
  public:
    audioplugin_base_t( const audioplugin_cfg_t& cfg );
    virtual ~audioplugin_base_t();
    virtual void ap_process(std::vector<wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::transport_t& tp) = 0;
    virtual void add_variables( TASCAR::osc_server_t* srv ) {};
    virtual void add_licenses( licensehandler_t* ) {};
    const std::string& get_name() const { return name; };
    const std::string& get_modname() const { return modname; };
  protected:
    std::string name;
    std::string modname;
  private:
    bool prepared;
  };

  class audioplugin_t : public audioplugin_base_t {
  public:
    audioplugin_t( const audioplugin_cfg_t& cfg );
    virtual ~audioplugin_t();
    virtual void ap_process(std::vector<wave_t>& chunk, const TASCAR::pos_t& pos, const TASCAR::transport_t& tp);
    virtual void prepare( chunk_cfg_t& );
    virtual void add_variables( TASCAR::osc_server_t* srv );
    virtual void add_licenses( licensehandler_t* srv );
    virtual void release();
    virtual void validate_attributes(std::string&) const;
  private:
    audioplugin_t(const audioplugin_t&);
    std::string plugintype;
    void* lib;
    TASCAR::audioplugin_base_t* libdata;
  };

}

#define REGISTER_AUDIOPLUGIN(x) TASCAR_PLUGIN( audioplugin_base_t, const audioplugin_cfg_t&, x )

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
