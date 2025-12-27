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

struct Demo : SokolEngine {
	sg_pipeline default_pip{};
	float radian = 0.0174532777777778;
	//cam info
	vf3d cam_pos{0, 2, 2};
	vf3d cam_dir;
	float cam_yaw=0;
	float cam_pitch=0;
	int anim_index = 0;

	sg_sampler sampler{};

	sg_view tex_blank{};
	sg_view tex_uv{};

	sg_view gui_image{};

	sg_pass_action display_pass_action{};

	Shape platform;

	bool contact_test = false;

	//player camera test
	vf3d player_pos, player_vel;
	float player_height = 0.25f;
	bool player_camera = false;

	struct {
		Shape shp;

		int num_x=0, num_y=0;
		int num_ttl=0;

		float anim_timer=0;
		int anim=0;
	} bb;

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

	void setupPlatform() {
		Mesh& m=platform.mesh;
		m=Mesh::makeCube();

		platform.tex=tex_uv;
		
		platform.scale={2, .25f, 2};
		platform.translation={0, -1, 0};
		platform.updateMatrixes();
	}

	void setupBillboard() {
		Mesh& m=bb.shp.mesh;
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

		bb.shp.translation={0, 1, 0};

		bb.shp.tex=getTexture("assets/spritesheet.png");
		bb.num_x=4, bb.num_y=4;
		bb.num_ttl=bb.num_x*bb.num_y;
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

		setupPlatform();

		setupBillboard();

		setup_Quad();

		setupDisplayPassAction();

		setupDefaultPipeline();
	}

#pragma region UPDATE HELPERS
	void handleCameraLooking(float dt) {
		//left/right
		if(getKey(SAPP_KEYCODE_LEFT).held) cam_yaw+=dt;
		if(getKey(SAPP_KEYCODE_RIGHT).held) cam_yaw-=dt;

		//up/down
		if(getKey(SAPP_KEYCODE_UP).held) cam_pitch+=dt;
		if(getKey(SAPP_KEYCODE_DOWN).held) cam_pitch-=dt;

		//clamp camera pitch
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;

		//turn player_camera off and on
		if (getKey(SAPP_KEYCODE_Z).pressed)
		{
			if (!player_camera) {
				
				player_vel = { 0, 0, 0 };
			}
			player_camera ^= true;
		}
	}

	void handleCameraMovement(float dt) {
		//move up, down
		if(getKey(SAPP_KEYCODE_SPACE).held) cam_pos.y+=4.f*dt;
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam_yaw), 0, std::cos(cam_yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam_pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam_pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam_pos-=4.f*dt*lr_dir;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		//polar to cartesian
		cam_dir=polar3D(cam_yaw, cam_pitch);

		handleCameraMovement(dt);
	}


	//make billboard always point at camera.
	void updateBillboard(float dt) {
		//move with player 
		vf3d eye_pos=bb.shp.translation;
		vf3d target=cam_pos;

		vf3d y_axis(0, 1, 0);
		vf3d z_axis=(target-eye_pos).norm();
		vf3d x_axis=y_axis.cross(z_axis).norm();
		y_axis=z_axis.cross(x_axis);
		
		//slightly different than makeLookAt.
		mat4& m=bb.shp.model;
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
			bb.anim = 1; //front
		}
		if (angle < -2.35 && angle < 2.35)
		{
			bb.anim = 4; //left
		}
		if (angle > -0.70 && angle < 0.70)
		{
			bb.anim = 8; //right
		}
		if (angle > 0.70 && angle < 2.35) 
		{
			bb.anim = 12; //back
		}
		//bb.anim_timer-=dt;
		//if(bb.anim_timer<0) {
		//	bb.anim_timer+=.5f;
		//
		//	//increment animation index and wrap
		//	bb.anim++;
		//	bb.anim%=bb.num_ttl;
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

	void updatePhysics(float dt)
	{
		if (player_camera)
		{
			contact_test = false;
			player_pos = cam_pos - vf3d(0, player_height, 0);
			float record = 0.5f;

			vf3d* closest = nullptr;
			for (const auto& t : platform.mesh.tris) {
				vf3d close_pt = platform.mesh.getClosePt(player_pos,
					platform.mesh.verts[t.a].pos,
					platform.mesh.verts[t.b].pos,
					platform.mesh.verts[t.c].pos
				);

				float dist_sq = (close_pt - player_pos).mag_sq();
				
				int i = 0;

				if (!closest && dist_sq < record)
				{
					
					record = dist_sq;
					closest = &close_pt;
				}
			}

			if (closest)
			{
				contact_test = true;
			}

			//localize point
			
			//contact_test = false;
			//player_pos = cam_pos - vf3d(0, player_height, 0);
			//float w = 1;
			//mat4 inv_model = mat4::inverse(platform.model);
			//vf3d pt = matMulVec(inv_model, player_pos, w);
			//
			//
			//
			//float record_sq = -1;
			//for (const auto& t : platform.mesh.tris) {
			//	vf3d close_pt = platform.mesh.getClosePt(pt,
			//		platform.mesh.verts[t.a].pos,
			//		platform.mesh.verts[t.b].pos,
			//		platform.mesh.verts[t.c].pos
			//	);
			//	float dist_sq = (close_pt - pt).mag_sq();
			//	if (record_sq < 0 || dist_sq < record_sq) {
			//		record_sq = dist_sq;
			//		pt = close_pt;
			//		contact_test = true;
			//		
			//		//norm = (b - a).cross(c - a).norm();
			//	}
			//}
		}

		

	}

	
	

#pragma endregion

	void userUpdate(float dt) {
		handleUserInput(dt);
		updateGui(dt);
		updateBillboard(dt);
		updatePhysics(dt);
	}

#pragma region RENDER HELPERS
	void renderPlatform(const mat4& view_proj) {
		sg_bindings bind{};
		bind.vertex_buffers[0]=platform.mesh.vbuf;
		bind.index_buffer=platform.mesh.ibuf;
		bind.samplers[SMP_default_smp]=sampler;
		bind.views[VIEW_default_tex]=platform.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(view_proj, platform.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//render entire texture.
		fs_params_t fs_params{};
		fs_params.u_tl[0]=0, fs_params.u_tl[1]=0;
		fs_params.u_br[0]=1, fs_params.u_br[1]=1;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3*platform.mesh.tris.size(), 1);
	}
	
	void renderBillboard(const mat4& view_proj) {
		sg_bindings bind{};
		bind.vertex_buffers[0]=bb.shp.mesh.vbuf;
		bind.index_buffer=bb.shp.mesh.ibuf;
		bind.samplers[SMP_default_smp]=sampler;
		bind.views[VIEW_default_tex]=bb.shp.tex;
		sg_apply_bindings(bind);

		//pass transformation matrix
		mat4 mvp=mat4::mul(view_proj, bb.shp.model);
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_mvp, mvp.m, sizeof(mvp.m));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		//which region of texture to sample?

		fs_params_t fs_params{};
		int row=bb.anim/bb.num_x;
		int col=bb.anim%bb.num_x;
		float u_left=col/float(bb.num_x);
		float u_right=(1+col)/float(bb.num_x);
		float v_top=row/float(bb.num_y);
		float v_btm=(1+row)/float(bb.num_y);
		fs_params.u_tl[0]=u_left;
		fs_params.u_tl[1]=v_top;
		fs_params.u_br[0]=u_right;
		fs_params.u_br[1]=v_btm;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));

		sg_draw(0, 3*bb.shp.mesh.tris.size(), 1);
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
		mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
		mat4 cam_view=mat4::inverse(look_at);

		//perspective
		mat4 cam_proj=mat4::makePerspective(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000);

		//premultiply transform
		mat4 cam_view_proj=mat4::mul(cam_proj, cam_view);

		sg_apply_pipeline(default_pip);
		
		renderPlatform(cam_view_proj);

		//renderBillboard(cam_view_proj);
		if (contact_test)
		{
			render_Quad();
		}

		sg_end_pass();
		
		sg_commit();
	}
};