# Tasks

Implement these. Try to make as efficient as possible to execute on the GPU. Efficient means execution efficiency & better convergence. 

1. Load geometry & ray cast against it
2. Compute AO
3. Compute direct illumination (with MIS)
4. Compute indirect illumination
5. Optional. Freely extend it so that you can show your skill off

Fill these functions. 
```
TEST_F(DeviceTest, RayCast)
{
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
```

# Structure

- ADL (OpenCL wrapper)
	See `adlTest/main.cpp` for the basic use of the library (buffer allocation, read write, kernel execution). 
- adlTest (The application)
	Based on google test. You can filter a test to run by `--gtest_filter=`. 

# Build & Unit Test

```
./tools/premake4/win/premake4.exe vs2015
```


