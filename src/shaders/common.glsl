vec3 uncharted2TonemapHelper(vec3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 uncharted2Tonemap(vec3 v)
{
    float exposure_bias = 2.0f;
    vec3 curr = uncharted2TonemapHelper(v * exposure_bias);

    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2TonemapHelper(W);
    return curr * white_scale;
}

vec3 toRadiance(vec4 rgbe_) {
    if(rgbe_ == vec4(0.0, 0.0, 0.0, 0.0)) {
        return vec3(rgbe_);
    }

	vec4 rgbe = rgbe_ * 255.0f;

	int exp = int(rgbe[3]) - 128;
	return vec3(ldexp((rgbe[0]+0.5)/256.0, exp), ldexp((rgbe[1]+0.5)/256.0, exp), ldexp((rgbe[2]+0.5)/256.0, exp));
}

vec3 toneMapping(in vec3 radiance) {
    // tone mapping
	return uncharted2Tonemap(radiance);
}