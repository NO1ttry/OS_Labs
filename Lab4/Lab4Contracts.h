#pragma once
#ifdef BUILDING_DLL
  #define API extern "C" __declspec(dllexport)
#else
  #define API extern "C" __declspec(dllimport)
#endif
API int gcd(int a, int b);
API float e(int x);