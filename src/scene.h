/**
   \file scene.h
   \brief "scene" provide classes for scene definition, without rendering functionality
   
   \ingroup libtascar
   \author Giso Grimm
   \date 2013

   \section license License (LGPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/
#ifndef SCENE_H
#define SCENE_H

#include "coordinates.h"
#include "async_file.h"
#include <string>
#include <vector>
#include <iostream>
#include "defs.h"
#include "xmlconfig.h"
#include "inputs.h"
#include "acousticmodel.h"

namespace TASCAR {

  /** \brief Components relevant for the spatial modelling
   */
  namespace Scene {

    class route_t : public scene_node_base_t {
    public:
      route_t();
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      std::string get_name() const {return name;};
      bool get_mute() const {return mute;};
      bool get_solo() const {return solo;};
      void set_name(const std::string& s) {name=s;};
      void set_mute(bool b) {mute=b;};
      void set_solo(bool b,uint32_t& anysolo);
    private:
      std::string name;
      bool mute;
      bool solo;
    };

    class rgb_color_t {
    public:
      rgb_color_t(double r_,double g_,double b_):r(r_),g(g_),b(b_){};
      rgb_color_t(const std::string& webc);
      rgb_color_t():r(0),g(0),b(0){};
      std::string str();
      double r, g, b;
    };

    class object_t : public route_t {
    public:
      object_t();
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      bool isactive(double time) const;
      pos_t get_location(double time) const;
      zyx_euler_t get_orientation(double time) const;
      rgb_color_t color;
      double starttime;
      double endtime;
      track_t location;
      euler_track_t orientation;
      pos_t dlocation;
      zyx_euler_t dorientation;
    };

    class mirror_t {
    public:
      mirror_t():c1(0),c2(0){};
      pos_t p;
      double c1;
      double c2;
    };

    class face_object_t : public object_t {
    public:
      face_object_t();
      pos_t get_closest_point(double t,pos_t p);
      mirror_t get_mirror(double t, pos_t src);
      void prepare(double fs, uint32_t fragsize);
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      double width;
      double height;
      double reflectivity;
      double damping;
    };

    class bg_amb_t : public route_t, public async_sndfile_t {
    public:
      bg_amb_t();
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      void prepare(double fs, uint32_t fragsize);
      std::string filename;
      double gain;
      unsigned int loop;
      double starttime;
      unsigned int firstchannel;
    };

    class src_object_t;

    class sound_t : public scene_node_base_t {
    public:
      sound_t(src_object_t* parent_);
      ~sound_t();
      void set_parent(src_object_t* parent_);
      void set_portnum(uint32_t portnum_);
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      pos_t get_pos_global(double t) const;
      void prepare(double fs, uint32_t fragsize);
      std::string getlabel();
      bool isactive(double t);
      bool get_mute() const;
      bool get_solo() const;
      std::string get_port_name() const;
      std::string get_connection() const { return connect;};
      TASCAR::Acousticmodel::pointsource_t* get_source() { return source;};
      uint32_t get_portnum() const { return portnum;};
    private:
      pos_t loc;
      double chaindist;
      src_object_t* parent;
      std::string name;
      std::string connect;
      uint32_t portnum;
      double gain;
      double gain_linear;
      // dynamically allocated source type. Allocated in "prepare",
      // type defined in xml_read:
      TASCAR::Acousticmodel::pointsource_t* source; 
      // sources should be pointsource, amb1source
      double fs_;
    };

    class src_object_t : public object_t {
    public:
      src_object_t(object_t* reference);
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      sound_t* add_sound();
      void prepare(double fs, uint32_t fragsize);
      std::vector<sound_t> sound;
      std::vector<TASCAR::Input::base_t*> inputs;
      void fill(int32_t tp_firstframe, bool tp_running);
    private:
      int32_t startframe;
    };

    class listener_t : public object_t {
    public:
      listener_t();
      ~listener_t();
      void read_xml(xmlpp::Element* e);
      void prepare(double fs, uint32_t fragsize);
      TASCAR::Acousticmodel::sink_t* get_sink() { return sink;};
    private:
      TASCAR::Acousticmodel::sink_t* sink;
    };

    class diffuse_reverb_t : public route_t {
    public:
      diffuse_reverb_t();
      double border_distance(pos_t p);
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      void prepare(double fs, uint32_t fragsize){};
      pos_t center;
      pos_t size;
      zyx_euler_t orientation;
    };

    class range_t : public scene_node_base_t {
    public:
      range_t();
      void read_xml(xmlpp::Element* e);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      void prepare(double fs, uint32_t fragsize){};
      std::string name;
      double start;
      double end;
    };

    class scene_t : public scene_node_base_t {
    public:
      scene_t();
      void read_xml(xmlpp::Element* e);
      void read_xml(const std::string& filename);
      void write_xml(xmlpp::Element* e,bool help_comments=false);
      src_object_t* add_source();
      std::vector<sound_t*> linearize_sounds();
      std::vector<Input::base_t*> linearize_inputs();
      void prepare(double fs, uint32_t fragsize);
      std::string description;
      std::string name;
      double duration;
      std::vector<src_object_t> srcobjects;
      //std::vector<bg_amb_t> bg_amb;
      //std::vector<diffuse_reverb_t> reverbs;
      //std::vector<face_object_t> faces;
      listener_t listener;
      double guiscale;
      void listener_orientation(zyx_euler_t o){listener.dorientation=o;};
      void listener_position(pos_t p){listener.dlocation = p;};
      void set_source_position_offset(const std::string& srcname,pos_t position);
      void set_source_orientation_offset(const std::string& srcname,zyx_euler_t position);
      uint32_t anysolo;
      void set_mute(const std::string& name,bool val);
      void set_solo(const std::string& name,bool val);
      bool get_playsound(const sound_t*);
      std::vector<range_t> ranges;
      bool loop;
    };

    scene_t xml_read_scene(const std::string& filename);
    void xml_write_scene(const std::string& filename, scene_t scene, const std::string& comment="", bool help_comments = false);

  }

}

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
