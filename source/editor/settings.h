#pragma once
#include "../vdrift/configfile.h"
#include "../ogre/common/settings_com.h"


#define SET_VER  2400  // 2.4

enum eShadowType  {  Sh_None=0, Sh_Depth, Sh_Soft  };


class SETTINGS : public SETcom
{
public:
///  params
//------------------------------------------
	int version;  // file version
	
	//  show
	bool trackmap, brush_prv;  int num_mini;
	float size_minimap;
	int tracks_view, tracks_sort;  bool tracks_sortup;

	//  graphics common
	int preset;
	int anisotropy, tex_filt, tex_size, ter_mtr, ter_tripl;
	float view_distance, terdetail, terdist, road_dist;
	bool horizon;
	
	float shadow_dist;  int shadow_size, lightmap_size, shadow_count, shadow_type;
	bool use_imposters, imposters_only;
	float grass, trees_dist, grass_dist;
	bool water_reflect, water_refract;  int water_rttsize;
	std::string shader_mode;

	class GameSet
	{
	public:
		//  track
		std::string track;  bool track_user;
		float trees;
	} gui;
	
	//  misc
	bool allow_save, inputBar, camPos;
	bool check_load, check_save;

	//  settings
	bool bFog, bTrees, bWeather;
	int ter_skip, mini_skip;  float road_sphr;
	float cam_speed, cam_inert, cam_x,cam_y,cam_z, cam_dx,cam_dy,cam_dz;
	
	//  ter generate
	float gen_scale, gen_ofsx,gen_ofsy, gen_freq, gen_persist, gen_pow;  int gen_oct;
	float gen_mul, gen_ofsh, gen_roadsm;
	float gen_terMinA,gen_terMaxA,gen_terSmA, gen_terMinH,gen_terMaxH,gen_terSmH;

	//  align ter
	float al_w_mul, al_w_add, al_smooth;
	//  tweak
	std::string tweak_mtr;
	//  pick
	bool pick_setpar;
	
//------------------------------------------
	SETTINGS();

	template <typename T>
	bool Param(CONFIGFILE & conf, bool write, std::string pname, T & value)
	{
		if (write)
		{	conf.SetParam(pname, value);
			return true;
		}else
			return conf.GetParam(pname, value);
	}
	void Serialize(bool write, CONFIGFILE & config);
	void Load(std::string sfile), Save(std::string sfile);
};
