#pragma OPENCL EXTENSION cl_amd_printf : enable

#define DIFFUSE 1
#define SPECULAR 2
#define BOUNCES 16
#define NUM_TRIANGLES 36
#define EPSILON 1e-6
#define PI 3.14159265359f
#define TWO_PI 6.28318530718f
#define INV_PI 0.31830988618f

typedef struct __attribute__ ((packed)) material
{
    float4 albedo;
    float4 emissive;
    float roughness;
    int type;
    char padding[24];
}Material;

typedef struct __attribute__ ((packed)) triangle
{
   float4 p1; //16
   float4 p2; //16
   float4 p3; //16
   int id; //4
   char padding[12];
}Triangle;

typedef struct
{
    float3 origin;
    float3 dir;
    float3 invDir;
    int sign[3];
}Ray;

typedef struct
{
  float t;
  float3 p; //Intersection point
  float3 n;
  const __global Triangle* intersectedTriangle;
}HitRecord;

//Wang Hash
unsigned int hashUInt32(unsigned int x)
{
   #if 0
    x = (x ^ 61) ^ (x >> 16);
    x = x + (x << 3);
    x = x ^ (x >> 4);
    x = x * 0x27d4eb2d;
    x = x ^ (x >> 15);
    return x;
#else
    return 1103515245 * x + 12345;
#endif
}

float getRandomFloat(unsigned int* seed)
{
    *seed = (*seed ^ 61) ^ (*seed >> 16);
    *seed = *seed + (*seed << 3);
    *seed = *seed ^ (*seed >> 4);
    *seed = *seed * 0x27d4eb2d;
    *seed = *seed ^ (*seed >> 15);
    *seed = 1103515245 * (*seed) + 12345;

    return (float)(*seed) * 2.3283064365386963e-10f;
}

Ray getRay(float3 origin, float3 dir)
{
    dir = normalize(dir);

    Ray r;
    r.origin = origin;
    r.dir = dir;
    r.invDir = 1.0f / dir;

    r.sign[0] = r.invDir.x < 0;
    r.sign[1] = r.invDir.y < 0;
    r.sign[2] = r.invDir.z < 0;

    return r;
}

bool intersectTriangle(Ray* r, const __global Triangle* triangle, HitRecord* record, float tmax)
{
 
    float3 e1 = triangle->p2.xyz - triangle->p1.xyz;
    float3 e2 = triangle->p3.xyz - triangle->p1.xyz;
   
   // Calculate planes normal vector
    float3 pvec = cross(r->dir, e2);
    float det = dot(e1, pvec);

    // Ray is parallel to plane
    if (det < 1e-8f || -det > 1e-8f)
    {
        return false;
    }

    float inv_det = 1.0f / det;
    float3 tvec = r->origin - triangle->p1.xyz;
    float u = dot(tvec, pvec) * inv_det;
    
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    float3 qvec = cross(tvec, e1);
    float v = dot(r->dir, qvec) * inv_det;
    
    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }

    float t = dot(e2, qvec) * inv_det;
    float3 norm = cross(e2, e1);

    if(t > 0.0f && t < tmax)
    {
      record->t = t;
      record->p = r->origin + r->dir * t;
      record->intersectedTriangle = triangle;
      record->n = normalize(u * norm + v * norm + (1.0f - u - v) * norm);
      return true;
    }

  return false;
}

bool intersectWorld(Ray* r, const __global Triangle* tBuffer, HitRecord* rec)
{
  float hitDistance = 1e20f;
  bool isHit = false;
  
  for(int i = 0; i < NUM_TRIANGLES ; i++)
  { 
     bool isIntersecting = intersectTriangle(r, &tBuffer[i], rec, hitDistance);
     
     if(isIntersecting)
     {
       hitDistance = rec->t;
       isHit = true;
     }
  }
 
  return isHit;
}

float3 reflect(float3 v, float3 n)
{
    return -v + 2.0f * dot(v, n) * n;
}

float3 sampleHemisphereCosine(float3 n, unsigned int* seed)
{
    float phi = TWO_PI * getRandomFloat(seed);
    float sinThetaSqr = getRandomFloat(seed);
    float sinTheta = sqrt(sinThetaSqr);

    float3 axis = fabs(n.x) > 0.001f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
    float3 t = normalize(cross(axis, n));
    float3 s = cross(n, t);

    return normalize(s * cos(phi) * sinTheta + t * sin(phi) * sinTheta + n * sqrt(1.0f - sinThetaSqr));
}

float distributionGGX(float cosTheta, float roughness)
{
    float roughness2 = roughness*roughness;
    return roughness2 * INV_PI / pow(cosTheta * cosTheta * (roughness2 - 1.0f) + 1.0f, 2.0f);
}

float3 sampleGGX(float3 n, float roughness, float* cosTheta, unsigned int* seed)
{
    float phi = TWO_PI * getRandomFloat(seed);
    float xi = getRandomFloat(seed);
    *cosTheta = sqrt((1.0f - xi) / (xi * (roughness * roughness - 1.0f) + 1.0f));
    float sinTheta = sqrt(max(0.0f, 1.0f - (*cosTheta) * (*cosTheta)));

    float3 axis = fabs(n.x) > 0.001f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
    float3 t = normalize(cross(axis, n));
    float3 s = cross(n, t);

    return normalize(s * cos(phi) * sinTheta + t * sin(phi) * sinTheta + n * (*cosTheta));
}

//Need to implement this 
float4 Brdf(float3 wo, float3* wi, float* pdf, float3 normal, const __global Material* mat, unsigned int* seed)
{
  if(mat->type == DIFFUSE)
  {
    *wi = sampleHemisphereCosine(normal, seed);
    
    *pdf = dot(*wi, normal) * INV_PI;
    
    return mat->albedo * INV_PI;
  }
  else if(mat->type == SPECULAR)
  {
    float cosTheta;
    float3 wh = sampleGGX(normal, mat->roughness, &cosTheta, seed);
    *wi = reflect(wo, wh);
    
    if (dot(*wi, normal) * dot(wo, normal) < 0.0f) return 0.0f;

    float D = distributionGGX(cosTheta, mat->roughness);

    *pdf = D * cosTheta / (4.0f * dot(wo, wh));
    
    return D / (4.0f * dot(*wi, normal) * dot(wo, normal)) * mat->albedo * 2.0f;
  }

  return (float4)(0.0f, 0.0f, 0.0f, 1.0f);;
}

float4 traceRays(Ray* r, const __global Triangle* tBuffer, const __global Material* matBuffer, unsigned int* seed)
{
  float4 radiance = 0.0f;
  float4 mask = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
  const float4 bgColor = (float4)(0.45f, 0.45f, 0.45f, 1.0f);

  for(unsigned int i = 0 ; i < BOUNCES; ++i)
  {
      HitRecord rec;

      if(!intersectWorld(r, tBuffer, &rec))
      {
         radiance += mask * max(bgColor , 0.0f);
         break;
      }

      const __global Material* material = &matBuffer[rec.intersectedTriangle->id];

      radiance += mask * material->emissive * 3.0f;

      rec.n = dot( rec.n, r->dir) < 0.0f ?  rec.n :  rec.n * (-1.0f);

      float3 wi;
      float3 wo = -r->dir;
      float pdf = 0.0f;
      
      float4 color = Brdf(wo, &wi, &pdf, rec.n, material , seed);
      
      if (pdf <= 0.0f) break;

      float4 factor = color * dot(wi, rec.n) / pdf;

      mask *= factor;

      *r = getRay(rec.p + wi * 0.01f, wi);
  }
  
  return max(radiance, 0.0f);
}

Ray generateRay(const int xCoord, const int yCoord, const int width, const int height, unsigned int* seed)
{
    float invWidth = 1.0f / (float)(width), invHeight = 1.0f / (float)(height);
    float aspectratio = (float)(width) / (float)(height);
    float fov = (60.0f * M_PI) / 180.0f;
    float angle = tan(0.5f * fov);

    const float3 eye = (float3)(0.0, 2.75, 4.0);
	const float3 center = eye + (float3)(0.0, 0.0, -1.0);
	const float3 up = (float3)(0.0, 1.0, 0.0);

    const float3 viewDir = normalize(center - eye);
	const float3 holDir = normalize(cross(viewDir, up));
	const float3 upDir = normalize(cross(holDir, viewDir));

    float x = (float)(xCoord) + getRandomFloat(seed) - 0.5f;
    float y = (float)(yCoord) + getRandomFloat(seed) - 0.5f;

    x = (2.0f * ((x + 0.5f) * invWidth) - 1) * angle * aspectratio;
    y = -(1.0f - 2.0f * ((y + 0.5f) * invHeight)) * angle;

    float3 dir = normalize(x * holDir + -1.0f * y * upDir + viewDir);
    float3 pointAimed = eye + 4.0f * dir;
    
    return getRay(eye, normalize(pointAimed - eye));
}

float4 gammaCorrect(float4 value)
{
    float4 t = pow(value, 1.0f / 2.2f);
    return (float4)(t.x, t.y, t.z, 1.0);
}

float4 readFromGamma(float4 value)
{
    float4 t = pow(value, 2.2f);
    return (float4)(t.x, t.y, t.z, 1.0f);
}

__kernel
void GenerateColors(__global Triangle* tBuffer, __global Material* matBuffer, __global float4* gDst, const int4 cRes )
{
	const int gi = get_global_id(0) % cRes.x;
	const int gj = get_global_id(0) / cRes.x;
 
    unsigned int seed = get_global_id(0) + hashUInt32(cRes.z) ;

    Ray r = generateRay(gi, gj, cRes.x, cRes.y, &seed);	
	    
	float4 finalcolor = traceRays(&r, tBuffer, matBuffer, &seed) ;

    if(cRes.z == 0)
    {
      gDst[ gj*cRes.x + gi ] = gammaCorrect(finalcolor);
    }
    else
    {
      gDst[ gj*cRes.x + gi ] = gammaCorrect((readFromGamma(gDst[ gj*cRes.x + gi ]) * (cRes.z - 1) + finalcolor) / cRes.z);
    }
}
