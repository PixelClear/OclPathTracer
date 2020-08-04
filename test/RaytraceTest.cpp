#include <test/TestBase.h>
#include <stdlib.h>
#include "SharedHeader.h"
#include <time.h> 

using namespace adl;
using namespace std;

struct int4
{
    int4() = default;
    int4(const int x_, const int y_, const int z_, const int w_) : x(x_), y(y_), z(z_), w(w_) {}
    
    int x, y, z, w;
};



cl_float4 cross(const cl_float4& v1, const cl_float4& v2)
{
    cl_float4 temp;

    temp.x = (v1.y * v2.z - v1.z * v2.y);
    temp.y = (v1.z * v2.x - v1.x * v2.z);
    temp.z = (v1.x * v2.y - v1.y * v2.x);

    return temp;
}

cl_float4 operator-(const cl_float4& v1, const cl_float4& v2)
{
    return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w };
}

cl_float4 operator/(const cl_float4& v1, cl_float scalar) {
    return { v1.x / scalar, v1.y / scalar, v1.z / scalar, v1.w / scalar };
}

double length(cl_float4 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

cl_float4 unit_vector(cl_float4 v)
{
    return v / length(v);
}

#define DIFFUSE 1
#define SPECULAR 2

//--------------------------Shared structs---------------------------------------
#pragma pack(1)
typedef struct
{
    cl_float4 albedo; //16
    cl_float4 emissive; //16
    cl_float roughness; //4
    cl_int  type;//4
    char padding[24];

}Material;

#pragma pack(1)
typedef struct triangle
{
    triangle() = default;

    triangle(cl_float4 p1_, cl_float4 p2_, cl_float4 p3_, cl_int id_) : p1(p1_), p2(p2_), p3(p3_), id(id_)
    {

    }

    cl_float4 p1; //16
    cl_float4 p2; //16
    cl_float4 p3; //16
    cl_int id; //4
    char padding[12]; //12
}Triangle;

u32 f2c( float a )
{
	a *= 255;
	u32 b = std::min( (int)a, 255 );
	return b;
}

float drand() { return (rand() / (RAND_MAX + 1.0f)); }

bool loadModel(std::vector<Triangle>& tBuffer, std::vector<Material>& materialBuffer)
{
    //Read File
    const char* filepath = "../test/cornellbox.bin";
    u64 sizeInByte = 0;
    std::ifstream i(filepath, std::ios::binary);
    std::streampos begin = i.tellg();
    i.seekg(0, std::ios::end);
    std::streampos end = i.tellg();
    i.close();
    sizeInByte = end - begin;

    //Prepare mesh vectors
    struct LocalMesh
    {
        float m_albedo;
        vector<int4> m_idx;
        vector<cl_float4> m_vtx;
        Material mat;
    };

    vector<LocalMesh*> meshes;
   
    ADLASSERT(sizeInByte, 0);
    {
        char* ptr = new char[sizeInByte];
        std::ifstream i(filepath, std::ios::binary);
        i.read((char*)ptr, sizeInByte);
        i.close();

        int* c = (int*)ptr;
        int n = c[0]; c++;

        for (int i = 0; i < n; i++)
        {
            LocalMesh* m = new LocalMesh;
            int nf = c[0]; c++;
            m->m_albedo = *(float*)(&c[0]); c++;

            m->m_idx.resize(nf);
            for (int j = 0; j < nf; j++)
            {
                m->m_idx[j].x = c[0]; c++;
                m->m_idx[j].y = c[0]; c++;
                m->m_idx[j].z = c[0]; c++;
                m->m_idx[j].w = c[0]; c++;
            }

            int nv = c[0]; c++;
            m->m_vtx.resize(nv);
            for (int j = 0; j < nv; j++)
            {
                m->m_vtx[j].x = *(float*)&c[0]; c++;
                m->m_vtx[j].y = *(float*)&c[0]; c++;
                m->m_vtx[j].z = *(float*)&c[0]; c++;
                m->m_vtx[j].w = *(float*)&c[0]; c++;
            }

            m->mat.type = DIFFUSE;

            if (m->m_albedo != 0.5f)
            {
                m->mat.emissive = { 30.0f, 30.0f, 30.0f ,1.0f };
                m->mat.albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
            }
            else
                m->mat.emissive = { 0.0f, 0.0f, 0.0f, 1.0f };


            meshes.push_back(m);
        }
        delete[] ptr;
    }

    cl_int id = 0;
    //Prepare Opencl buffers
    for (uint32_t i = 0; i < meshes.size(); i++)
    {
        if (i == 0 || i == 1 || i == 2)
            meshes[i]->mat.albedo = { 0.7, 0.7, 0.7, 1.0 };
        if (i == 3)
            meshes[i]->mat.albedo = { 0.6, 0.0, 0.0 , 1.0f};
        if (i == 4)
            meshes[i]->mat.albedo = { 0.0, 0.6, 0.0 , 1.0f };
        if (i == 5)
        {
            meshes[i]->mat.albedo = { 0.5f, 0.35f, 0.05f };
            meshes[i]->mat.roughness = 0.008f;
            meshes[i]->mat.type = SPECULAR;
        }
       

        for (uint32_t j = 0; j < meshes[i]->m_idx.size(); j++)
        {
            cl_float4 p1 = {meshes[i]->m_vtx[meshes[i]->m_idx[j].x].x , meshes[i]->m_vtx[meshes[i]->m_idx[j].x].y , meshes[i]->m_vtx[meshes[i]->m_idx[j].x].z , 0.0f };
            cl_float4 p2 = { meshes[i]->m_vtx[meshes[i]->m_idx[j].y].x, meshes[i]->m_vtx[meshes[i]->m_idx[j].y].y, meshes[i]->m_vtx[meshes[i]->m_idx[j].y].z, 0.0f };
            cl_float4 p3 = { meshes[i]->m_vtx[meshes[i]->m_idx[j].z].x, meshes[i]->m_vtx[meshes[i]->m_idx[j].z].y, meshes[i]->m_vtx[meshes[i]->m_idx[j].z].z, 0.0f };
            cl_float4 p4 = { meshes[i]->m_vtx[meshes[i]->m_idx[j].w].x, meshes[i]->m_vtx[meshes[i]->m_idx[j].w].y , meshes[i]->m_vtx[meshes[i]->m_idx[j].w].z, 0.0f };
            
            Triangle t1{ p1, p2, p3 , id };
            Triangle t2{ p3, p4, p1 , id };

            tBuffer.push_back(t1);
            tBuffer.push_back(t2);
            materialBuffer.push_back(meshes[i]->mat);

            id++;
        }
    }

    return (tBuffer.size()/2 == materialBuffer.size());
}



TEST_F(DeviceTest, RayCast)
{
    std::vector<Triangle> triangles;
    std::vector<Material> materials;

    triangles.reserve(18 * 2);
    materials.reserve(18);

    if (!loadModel(triangles, materials))
    {
        printf("Error loading model !!\n");
        return;
    }

    //Prepare buffers for Opencl
    Buffer<Triangle>* tBuffer;
    Buffer<Material>* materialBuffer;
    const int dimension = 512;
    Buffer<cl_float4> frameBuff(m_d, dimension*dimension);
   
    tBuffer = new Buffer<Triangle>(m_d, triangles.size() * sizeof(Triangle));
    materialBuffer = new Buffer<Material>(m_d, materials.size() * sizeof(Material));;

    Triangle* tb = tBuffer->getHostPtr();
    Material* mb = materialBuffer->getHostPtr();

    DeviceUtils::waitForCompletion(m_d);

    Triangle *ttb = tb;
    Material *tmb = mb;

    for (size_t i = 0; i < triangles.size(); i++)
    {
        ttb[i] = triangles[i];
    }

    for (size_t i = 0; i < materials.size(); i++)
    {
        tmb[i] = materials[i];
    }

    tBuffer->returnHostPtr(tb);
    materialBuffer->returnHostPtr(mb);
   
    DeviceUtils::waitForCompletion(m_d);

    unsigned int frameCount = 0;
   
    while (frameCount != 10000)
    {
        int4 res;
        res.x = dimension; res.y = dimension; res.z = frameCount++;
        BufferInfo bInfo[] = { BufferInfo(tBuffer), BufferInfo(materialBuffer), BufferInfo(&frameBuff) };
        Launcher launcher(m_d, m_d->getKernel(SELECT_KERNELPATH1(m_d, "../test/", "GenerateColors"), "GenerateColors"));

        //set buffers
        launcher.setBuffers(bInfo, sizeof(bInfo) / sizeof(BufferInfo));
        
        //set resolution constant 
        launcher.setConst(res);
        
        //Launch 1D kernel
        launcher.launch1D(dimension * dimension);

        //Wait for kernel to finish
        DeviceUtils::waitForCompletion(m_d);
    }

    //Save rendering to file
    {
        cl_float4* h = frameBuff.getHostPtr();
        DeviceUtils::waitForCompletion(m_d);
        char path[256];
        getFilePath("rayCastAo", "ppm", path);

        FILE *f = fopen(path, "w");
        fprintf(f, "P3\n%d %d\n%d\n", dimension, dimension, 255);

        for (int i = 0; i < dimension * dimension; i++)
        {
            cl_float4 v = h[i];
            cl_float3 color = { std::sqrtf(v.x), std::sqrtf(v.y), std::sqrtf(v.z) };
            fprintf(f, "%d %d %d ", f2c(color.x), f2c(color.y), f2c(color.z));
        }

        fclose(f);

        DeviceUtils::waitForCompletion(m_d);
    }
}

TEST_F(DeviceTest, AmbientOcclusion)
{
}

TEST_F(DeviceTest, DirectIllumination)
{
}

TEST_F(DeviceTest, IndirectIllumination)
{
}

