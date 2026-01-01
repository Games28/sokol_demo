#define SOKOL_GLCORE
#include "sokol_engine.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include <iostream>

#include "shd.glsl.h"

#include "math/v3d.h"
#include "math/mat4.h"

#include "mesh.h"

//for time
#include <ctime>

#include "texture_utils.h"

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

struct Shape {
	Mesh mesh;

	sg_view tex{};
	

	vf3d scale{1, 1, 1}, rotation, translation;
	mat4 model=mat4::makeIdentity();

	void updateMatrixes() {
		//xyz euler angles?
		mat4 rot_x=mat4::makeRotX(rotation.x);
		mat4 rot_y=mat4::makeRotY(rotation.y);
		mat4 rot_z=mat4::makeRotZ(rotation.z);
		mat4 rot=mat4::mul(rot_z, mat4::mul(rot_y, rot_x));

		mat4 scl=mat4::makeScale(scale);

		mat4 trans=mat4::makeTranslation(translation);

		//combine
		model=mat4::mul(trans, mat4::mul(rot, scl));
	}
};

struct Object
{
	Mesh mesh;
	sg_view tex{ SG_INVALID_ID };
	bool draggable = false;
	bool isbillboard = false;

	vf3d scale{ 1, 1, 1 }, rotation, translation;
	mat4 model = mat4::makeIdentity();
	int num_x = 0, num_y = 0;
	int num_ttl = 0;

	float anim_timer = 0;
	int anim = 0;

	void updateMatrixes() {
		//xyz euler angles?
		mat4 rot_x = mat4::makeRotX(rotation.x);
		mat4 rot_y = mat4::makeRotY(rotation.y);
		mat4 rot_z = mat4::makeRotZ(rotation.z);
		mat4 rot = mat4::mul(rot_z, mat4::mul(rot_y, rot_x));

		mat4 scl = mat4::makeScale(scale);

		mat4 trans = mat4::makeTranslation(translation);

		//combine
		model = mat4::mul(trans, mat4::mul(rot, scl));
	}
};

struct
{
	vf3d pos{ 0,2,2 };
	vf3d dir;
	float yaw = 0;
	float pitch = 0;
	mat4 proj, view;
	mat4 view_proj;
}cam;

struct Light
{
	vf3d pos;
	sg_color col;

};

struct Demo : SokolEngine {
	sg_pipeline default_pip{};
	float radian = 0.0174532777777778;
	//cam info
	vf3d cam_pos{0, 2, 2};
	vf3d movement{ 0,0,0 };
	vf3d cam_dir;
	float cam_yaw=0;
	float cam_pitch=0;
	int anim_index = 0;

	sg_sampler sampler{};

	//grab object test
	vf3d current_dir, prevous_dir;
	Object* held_obj = nullptr;
	vf3d grab_ctr, grab_norm;

	mat4 cam_view_proj;
	mat4 mat_vew, mat_proj;
	std::vector<Light> lights;
	Light* mainlight;

	std::vector<Object> objects;
	
	const std::vector<std::string> Structurefilenames{
		"assets/models/deserttest.txt",
		"assets/models/sandspeeder.txt",
		"assets/models/tathouse1.txt",
		"assets/models/tathouse2.txt",
	};

	sg_view tex_blank{};
	sg_view tex_uv{};

	sg_view gui_image{};

	sg_pass_action display_pass_action{};

	Shape platform;

	bool contact_test = false;

	//player camera test
	vf3d player_pos, player_vel, gravity = { 0,-1,0 };
	float player_height = 0.25f, player_rad = 0.1f;
	bool player_camera = false, player_on_ground = false;





	struct
	{
		Shape shp;

		int num_x = 0, num_y = 0;
		int num_ttl = 0;

		float anim_timer = 0;
		int anim = 0;

		sg_pipeline pip{};
		sg_bindings bind{};
		sg_view gui_image{};

	}gGui;

#pragma region SETUP HELPERS
	void setupEnvironment() {
		sg_desc desc{};
		desc.environment=sglue_environment();
		sg_setup(desc);
	}

	void setupLights()
	{
		//white
		lights.push_back({ {-1,3,1},{1,1,1,1} });
		mainlight = &lights.back();
		
	}
	
	//primitive textures to debug with
	void setupTextures() {
		tex_blank=makeBlankTexture();
		tex_uv=makeUVTexture(512, 512);
	}

	//if texture loading fails, default to uv tex.
	sg_view getTexture(const std::string& filename) {
		sg_view tex;
		auto status=makeTextureFromFile(tex, filename);
		if(!status.valid) tex=tex_uv;
		return tex;
	}

	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
	}

	void setupObjects()
	{
		Object b;
		Mesh& m = b.mesh;
		auto status = Mesh::loadFromOBJ(m, Structurefilenames[0]);
		if (!status.valid) m = Mesh::makeCube();
		b.scale = { 1,1,1 };
		b.translation = { 0,-2,0 };
		b.updateMatrixes();
		b.tex = getTexture("assets/poust_1.png");
		objects.push_back(b);
	}

	void setupPlatform() {
		Object obj;
		Mesh& m=obj.mesh;
		m=Mesh::makeCube();

		obj.tex = getTexture("assets/poust_1.png");
		
		obj.scale={10, .25f, 10};
		obj.translation={0, -1, 0};
		obj.updateMatrixes();
		objects.push_back(obj);
	}

	void setupBillboard() {
		Object obj;
		Mesh& m=obj.mesh;
		m.verts={
			{{-.5f, .5f, 0}, {0, 0, 1}, {0, 0}},//tl
			{{.5f, .5f, 0}, {0, 0, 1}, {1, 0}},//tr
			{{-.5f, -.5f, 0}, {0, 0, 1}, {0, 1}},//bl
			{{.5f, -.5f, 0}, {0, 0, 1}, {1, 1}}//br
		};
		m.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		m.updateVertexBuffer();
		m.updateIndexBuffer();

		obj.translation={0, 1, 0};
		obj.isbillboard = true;
		obj.draggable = true;

		obj.tex=getTexture("assets/spritesheet.png");
		obj.num_x=4, obj.num_y=4;
		obj.num_ttl= obj.num_x*obj.num_y;
		objects.push_back(obj);
	}

	void setup_Quad()
	{
		//2d texture quad
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_texview_v_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_texview_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(texview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		gGui.pip = sg_make_pipeline(pip_desc);

		//quad vertex buffer: xyuv
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 0}},//tl
			{{1, -1}, {1, 0}},//tr
			{{-1, 1}, {0, 1}},//bl
			{{1, 1}, {1, 1}}//br
		};

		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr = vertexes;
		vbuf_desc.data.size = sizeof(vertexes);
		gGui.bind.vertex_buffers[0] = sg_make_buffer(vbuf_desc);
		gGui.bind.samplers[SMP_texview_smp] = sampler;

		gGui.gui_image = getTexture("assets/animation_test.png");

		//setup texture animatons
		gGui.num_x = 5; gGui.num_y = 2;
		gGui.num_ttl = gGui.num_x * gGui.num_y;
	
	}

	//clear to bluish
	void setupDisplayPassAction() {
		display_pass_action.colors[0].load_action=SG_LOADACTION_CLEAR;
		display_pass_action.colors[0].clear_value={.25f, .45f, .65f, 1.f};
	}

	void setupDefaultPipeline() {
		sg_pipeline_desc pipeline_desc{};
		pipeline_desc.layout.attrs[ATTR_default_v_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_default_v_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_default_v_uv].format=SG_VERTEXFORMAT_FLOAT2;
		pipeline_desc.shader=sg_make_shader(default_shader_desc(sg_query_backend()));
		pipeline_desc.index_type=SG_INDEXTYPE_UINT32;
		pipeline_desc.cull_mode=SG_CULLMODE_FRONT;
		pipeline_desc.depth.write_enabled=true;
		pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		default_pip=sg_make_pipeline(pipeline_desc);
	}
#pragma endregion

	void userCreate() override {
		setupEnvironment();

		setupTextures();

		setupSampler();

		setupLights();
		//setupPlatform();
		setupObjects();

		setupBillboard();

		setup_Quad();

		setupDisplayPassAction();

		setupDefaultPipeline();
	}

#pragma region UPDATE HELPERS
	
	vf3d rayIntersectPlane(const vf3d& a, const vf3d& b, const vf3d& ctr, const vf3d& norm, float* tp = nullptr)
	{
		float t = norm.dot(ctr - a) / norm.dot(b - a);
		if (tp) *tp = t;
		return a + t * (b - a);
	}

	float intersectRay(Object& obj,const vf3d& orig_world, const vf3d& dir_world)
	{
		float w = 1;
		mat4 inv_model = mat4::inverse(obj.model);
		vf3d orig_local = matMulVec(inv_model, orig_world, w);
		w = 0;
		vf3d dir_local = matMulVec(inv_model, dir_world, w);

		dir_local = dir_local.norm();

		float record = -1;
		for (const auto& t : obj.mesh.tris) {
			float dist = obj.mesh.rayIntersectTri(
				orig_local,
				dir_local,
				obj.mesh.verts[t.a].pos,
				obj.mesh.verts[t.b].pos,
				obj.mesh.verts[t.c].pos
			);

			if (dist < 0) continue;

			//sort while iterating
			if (record < 0 || dist < record) record = dist;


		}

		if (record < 0) return -1;

		vf3d p_local = orig_local + record * dir_local;
		w = 1;
		vf3d p_world = matMulVec(obj.model, p_local, w);
		return(p_world - orig_world).mag();
	}

	void handleGrabActionBegin()
	{
		contact_test = false;
		handleGrabActionEnd();
		//contact_test = false;
		float record = -1;
		Object* close_obj = nullptr;
		for (auto& o : objects)
		{
			if (o.isbillboard)
			{
				float dist = intersectRay(o, cam.pos, current_dir);
				if (dist < 0) continue;
				//"sort" while iterating
				if (record < 0 || dist < record) {
					record = dist;
					close_obj = &o;
					contact_test = true;
				}
			}
		}
		if (!close_obj) return;
		if (!close_obj->draggable) return;
		if (close_obj)
		{
			held_obj = close_obj;
		}

		grab_ctr = cam.pos + record * current_dir;
		grab_norm = cam.dir;
			
		
	}

	void handleGrabActionEnd()
	{
		held_obj = nullptr;
	}

	void handleGrabActionUpdate()
	{
		if (!held_obj) return;

		vf3d prev_pt = rayIntersectPlane(cam.pos, prevous_dir, grab_ctr, grab_norm);
		vf3d curr_pt = rayIntersectPlane(cam.pos, current_dir, grab_ctr, grab_norm);
		held_obj->translation += curr_pt - prev_pt;
		held_obj->updateMatrixes();
	}

	void updateCameraRay()
	{
		prevous_dir = current_dir;

		
		float ndc_x = 2 * cam.pos.x / sapp_widthf() - 1;
		float ndc_y = 1 - 2 * cam.pos.y / sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w = 1;
		vf3d world = matMulVec(cam_view_proj , clip, w);
		world /= w;

		current_dir = (world - cam.pos).norm();

	}

	void handleCameraLooking(float dt) {
		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam.yaw+=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam.yaw-=dt;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam.pitch+=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam.pitch-=dt;

		//clamp camera pitch
		if(cam.pitch>Pi/2) cam.pitch=Pi/2-.001f;
		if(cam.pitch<-Pi/2) cam.pitch=.001f-Pi/2;

		//turn player_camera off and on
		if (getKey(SAPP_KEYCODE_Z).pressed)
		{
			if (!player_camera) {
				
				player_vel = { 0, 0, 0 };
				player_on_ground = false;
			}
			player_camera ^= true;
		}
	}

	void handleCameraMovement(float dt) {

		
		if (!player_camera)
		{
			//move up, down
			if (getKey(SAPP_KEYCODE_SPACE).held) cam.pos.y += 4.f * dt;
			if (getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam.pos.y -= 4.f * dt;
		}

		//move forward, backward
		vf3d fb_dir(std::sin(cam.yaw), 0, std::cos(cam.yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam.pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam.pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam.pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam.pos-=4.f*dt*lr_dir;

	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		//grab and drag with camera 
		{
			const auto grab_action = getKey(SAPP_KEYCODE_F);
			if (grab_action.pressed)
			{
				handleGrabActionBegin();
			}
			if (grab_action.held)
			{
				handleGrabActionUpdate();
			}
			if (grab_action.released)
			{
				handleGrabActionEnd();
			}
		}
		//polar to cartesian
		cam.dir=polar3D(cam.yaw, cam.pitch);
		if (getKey(SAPP_KEYCODE_R).held) mainlight->pos = cam.pos;

		handleCameraMovement(dt);
	}


	//make billboard always point at camera.
	void updateBillboard(Object& obj, float dt) {
		//move with player 
		vf3d eye_pos= obj.translation;
		vf3d target=cam.pos;

		vf3d y_axis(0, 1, 0);
		vf3d z_axis=(target-eye_pos).norm();
		vf3d x_axis=y_axis.cross(z_axis).norm();
		y_axis=z_axis.cross(x_axis);
		
		//slightly different than makeLookAt.
		mat4& m= obj.model;
		m(0, 0)=x_axis.x, m(0, 1)=y_axis.x, m(0, 2)=z_axis.x, m(0, 3)=eye_pos.x;
		m(1, 0)=x_axis.y, m(1, 1)=y_axis.y, m(1, 2)=z_axis.y, m(1, 3)=eye_pos.y;
		m(2, 0)=x_axis.z, m(2, 1)=y_axis.z, m(2, 2)=z_axis.z, m(2, 3)=eye_pos.z;
		m(3, 3)=1;
		
		float angle = atan2f(z_axis.z, z_axis.x);
		//
		//int i = 0;
		//
		if (angle < -0.70 && angle > -2.35 )
		{
			obj.anim = 1; //front
		}
		if (angle < -2.35 && angle < 2.35)
		{
			obj.anim = 4; //left
		}
		if (angle > -0.70 && angle < 0.70)
		{
			obj.anim = 8; //right
		}
		if (angle > 0.70 && angle < 2.35) 
		{
			obj.anim = 12; //back
		}
		//obj.anim_timer-=dt;
		//if(obj.anim_timer<0) {
		//	obj.anim_timer+=.5f;
		//
		//	//increment animation index and wrap
		//	obj.anim++;
		//	obj.anim%=obj.num_ttl;
		//}
	}

	void updateGui(float dt)
	{
		gGui.anim_timer -= dt;
		if (gGui.anim_timer < 0)
		{
			gGui.anim_timer += .5f;

			//increment animation index and wrap
			gGui.anim++;
			gGui.anim %= gGui.num_ttl;
		}
	}

	void updatePhysics(Object& obj,float dt)
	{
		if (player_camera && !obj.isbillboard)
		{
			player_pos = cam.pos - vf3d(0, player_height, 0);

			//contact_test = false;
			float w = 1;
			mat4 inv_model = mat4::inverse(obj.model);
			vf3d pt = matMulVec(inv_model, player_pos, w);
			float record = 0.5f;
			float dist_sq = 0;

			if (!player_on_ground)
			{
				player_vel += gravity * dt;
				player_pos += player_vel * dt;
			}

			vf3d* closest = nullptr;
			for (const auto& t : obj.mesh.tris) {
				vf3d close_pt = obj.mesh.getClosePt(pt,
					obj.mesh.verts[t.a].pos,
					obj.mesh.verts[t.b].pos,
					obj.mesh.verts[t.c].pos
				);

				dist_sq = (close_pt - pt).mag_sq();
				
				

				if (!closest && dist_sq < record)
				{
					
					record = dist_sq;
					closest = &close_pt;
				}

				int i = 0;
				if (dist_sq < player_rad * player_rad)
				{
					vf3d norm = closest->norm();
					float fix = player_rad - std::sqrtf(dist_sq);
					player_pos += fix * norm;
					player_vel = { 0,0,0 };
					player_on_ground = true;
				}
				//else
				//{
				//	player_on_ground = false;
				//}
			}

			if (player_on_ground)
			{
				if (closest)
				{
					vf3d norm = closest->norm();
					player_pos -= norm * norm.dot(player_pos);
				}
			}



			cam.pos = player_pos + vf3d(0, player_height, 0);
			
		}

		

	}

	
	

#pragma endregion

	void userUpdate(float dt) {
		
		handleUserInput(dt);
		updateGui(dt);

		updateCameraRay();

		for (auto& obj : objects)
		{
			if (obj.isbillboard)
			{
				updateBillboard(obj, dt);
				
			}
			updatePhysics(obj, dt);
		}

		
		
		
	}

#pragma region RENDER HELPERS
	void renderPlatform(Object& obj,const mat4& view_proj) {
		sg_bindings bind{};
		bind.vertex_buffers[0]=obj.mesh.vbuf;
		bind.index_buffer= obj.mesh.ibuf;
		bind.samplers[SMP_default_smp]=sampler;
		bind.views[VIEW_default_tex] = obj.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(view_proj, obj.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_model, obj.model.m, sizeof(vs_params.u_model));
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//render entire texture.
		//fs_params_t fs_params{};
		//lighting test
		fs_params_t fs_params{};
		{

			fs_params.u_num_lights = lights.size();
			int idx = 0;
			for (const auto& l : lights)
			{
				fs_params.u_light_pos[idx][0] = l.pos.x;
				fs_params.u_light_pos[idx][1] = l.pos.y;
				fs_params.u_light_pos[idx][2] = l.pos.z;
				fs_params.u_light_col[idx][0] = l.col.r;
				fs_params.u_light_col[idx][1] = l.col.g;
				fs_params.u_light_col[idx][2] = l.col.b;
				idx++;
			}
		}

		fs_params.u_view_pos[0] = cam.pos.x;
		fs_params.u_view_pos[1] = cam.pos.y;
		fs_params.u_view_pos[2] = cam.pos.z;
		//sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));


		fs_params.u_tl[0]=0, fs_params.u_tl[1]=0;
		fs_params.u_br[0]=1, fs_params.u_br[1]=1;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3* obj.mesh.tris.size(), 1);
	}
	
	void renderBillboard(Object& obj,const mat4& view_proj) {
		sg_bindings bind{};
		bind.vertex_buffers[0]= obj.mesh.vbuf;
		bind.index_buffer= obj.mesh.ibuf;
		bind.samplers[SMP_default_smp]=sampler;
		bind.views[VIEW_default_tex]= obj.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(view_proj, obj.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//which region of texture to sample?

		fs_params_t fs_params{};
		int row= obj.anim/ obj.num_x;
		int col= obj.anim% obj.num_x;
		float u_left=col/float(obj.num_x);
		float u_right=(1+col)/float(obj.num_x);
		float v_top=row/float(obj.num_y);
		float v_btm=(1+row)/float(obj.num_y);
		fs_params.u_tl[0]=u_left;
		fs_params.u_tl[1]=v_top;
		fs_params.u_br[0]=u_right;
		fs_params.u_br[1]=v_btm;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3* obj.mesh.tris.size(), 1);
	}

	void render_Quad()
	{
		//separate animation stuff later
		

		int row = gGui.anim / gGui.num_x;
		int col = gGui.anim % gGui.num_x;
		float u_left = col / float(gGui.num_x);
		float u_right = (1 + col) / float(gGui.num_x);
		float v_top = row / float(gGui.num_y);
		float v_btm = (1 + row) / float(gGui.num_y);

		sg_apply_pipeline(gGui.pip);

		gGui.bind.views[VIEW_texview_tex] = gGui.gui_image;
		sg_apply_bindings(gGui.bind);

		fs_texview_params_t fs_tex_params{};
		fs_tex_params.u_tl[0] = u_left;
		fs_tex_params.u_tl[1] = v_top;
		fs_tex_params.u_br[0] = u_right;
		fs_tex_params.u_br[1] = v_btm;

		sg_apply_uniforms(UB_fs_texview_params, SG_RANGE(fs_tex_params));
		sg_apply_viewport(2, 2, 100, 100, true);


		//4 verts = 1quad
		sg_draw(0, 4, 1);

	}

	

#pragma endregion
	
	void userRender() {
		sg_pass pass{};
		pass.action=display_pass_action;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		//camera transformation matrix
		mat4 look_at=mat4::makeLookAt(cam.pos, cam.pos+cam.dir, {0, 1, 0});
		
		mat4 cam_view=mat4::inverse(look_at);


		//perspective
		mat4 cam_proj=mat4::makePerspective(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000);
		
		//premultiply transform
		 cam_view_proj=mat4::mul(cam_proj, cam_view);

		sg_apply_pipeline(default_pip);

		
		

		for (auto& obj : objects)
		{
			if (obj.isbillboard)
			{
				renderBillboard(obj, cam_view_proj);
			}
			renderPlatform(obj, cam_view_proj);
			
		}
		if (contact_test)
		{
			render_Quad();
		}

		sg_end_pass();
		
		sg_commit();
	}
};