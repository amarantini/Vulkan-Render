#define M_PI 3.1415926535897932384626433832795
#define MAX_LIGHT_COUNT 10
#define HEIGHT_SCALE 0.1

/* ---------------------- Tone mapping ---------------------- */
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

vec3 toneMapping(in vec3 radiance) {
    // tone mapping
	return uncharted2Tonemap(radiance);
}

/* ---------------------- RGBE ---------------------- */
vec3 toRadiance(vec4 rgbe_) {
    if(rgbe_ == vec4(0.0, 0.0, 0.0, 0.0)) {
        return vec3(rgbe_);
    }

	vec4 rgbe = rgbe_ * 255.0f;

	int exp = int(rgbe[3]) - 128;
	return vec3(ldexp((rgbe[0]+0.5)/256.0, exp), ldexp((rgbe[1]+0.5)/256.0, exp), ldexp((rgbe[2]+0.5)/256.0, exp));
}

/* ---------------------- Light ---------------------- */

struct SphereLight { //sphere
    // a light that emits light in all directions equally
    // has location
    vec4 pos; //world space
    vec4 color; //tint * power
    vec4 others;//radius, limit, *, *
    vec4 shadow;
};

struct SpotLight { //spot
    // a light emits light in a cone shape
    // has position
    mat4 lightVP;
    vec4 pos; //world space, calculate using model * vec4(0,0,0,1)
    vec4 direction;  //world space, calculate using model * vec4(0,0,-1,0)
    vec4 color; //tint * power
    vec4 others;
    vec4 shadow;
};

struct DirectionalLight { //sun 
    // a light infinitely far away and emits light in one direction only
    vec4 direction; 
    vec4 color; //tint * power
    vec4 others;
    //float angle;
};


// R = reflection of view direction
vec3 calculateClosestPoint(vec3 lightPos, vec3 fragPos, vec3 R, float radius) {
    vec3 L = lightPos - fragPos;
    vec3 centerToRay = dot(L, R)*R - L;
    vec3 closestPoint = L + centerToRay * clamp(radius / length(centerToRay), 0.0f, 1.0f);
    return closestPoint;
}

float calculateFalloff(float distance, float radius) {
    if(distance < radius) {
		return 1.0;
	}
    float attenuation = 1.0 / (distance * distance);
    return attenuation;
    /*float numerator = clamp(1-pow(distance/radius,4), 0.0, 1.0);
    numerator *= numerator;
    float denumerator = distance * distance + 1;
    return numerator / denumerator;
    */
}

float calculateLimit(float distance, float limit) {
    if(limit>0) {
		return max(0, 1-pow(distance/limit,4));
	} else {
        return 1.0;
    }
}

