@vs vs_default

layout(binding=0) uniform vs_params {
	mat4 u_mvp;
};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

out vec2 uv;

void main() {
	gl_Position=u_mvp*vec4(v_pos, 1);
	uv=v_uv;
}

@end

@fs fs_default

layout(binding=0) uniform texture2D default_tex;
layout(binding=0) uniform sampler default_smp;

layout(binding=1) uniform fs_params {
	vec2 u_tl;
	vec2 u_br;
};

in vec2 uv;

out vec4 frag_color;

void main() {
	vec4 col=texture(
		sampler2D(default_tex, default_smp),
		u_tl+uv*(u_br-u_tl)
	);
	frag_color=vec4(col.rgb, 1);
}

@end

@program default vs_default fs_default

///////// 2d texture with/without animation //////////////////

@vs vs_texview

in vec2 v_pos;
in vec2 v_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(v_pos, .5, 1);
	uv.x = v_uv.x;
	uv.y = -v_uv.y;
}

@end

@fs fs_texview

layout(binding = 0) uniform texture2D texview_tex;
layout(binding = 0) uniform sampler texview_smp;

layout(binding = 0) uniform fs_texview_params
{
	vec2 u_tl;
	vec2 u_br;
};

in vec2 uv;

out vec4 frag_color;

void main()
{
	vec4 col = texture(sampler2D(texview_tex, texview_smp), u_tl + uv * (u_br - u_tl));
	frag_color = vec4(col.rgb, 1);
}

@end

@program texview vs_texview fs_texview
