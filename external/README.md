# x64 라이브러리

| 라이브러리 | 사용 파일 | 원본과 라이선스 |
|---|---|---|
| GLEW 2.3.1 | 헤더, `glew32.lib`, `glew32.dll` | [공식 소스](https://github.com/nigels-com/glew/tree/glew-2.3.1) · [라이선스](glew-2.3.1/LICENSE.txt) |
| GLFW 3.5.1 | 헤더, `glfw3.lib` | [공식 소스](https://github.com/glfw/glfw/tree/3.5.1) · [라이선스](glfw/LICENSE.md) |
| GLM 1.0.3 | `glm/` 헤더 | [공식 소스](https://github.com/g-truc/glm/tree/1.0.3) · [라이선스](glm/copying.txt) |
| Assimp 6.0.6 | OBJ 임포터용 헤더, `assimp-vc145-mt.lib`, `assimp-vc145-mt.dll` | [공식 릴리스](https://github.com/assimp/assimp/releases/tag/v6.0.6) · [라이선스](assimp/LICENSE) · [동봉된 라이선스](assimp/licenses/) |

GLEW·GLFW·GLM은 `Pratice_Grapic/external/`의 사본이다. Assimp는 공식 v6.0.6 소스에서 MSVC v145로 빌드했다. `ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF`, `ASSIMP_BUILD_OBJ_IMPORTER=ON`, `ASSIMP_NO_EXPORT=ON`, `BUILD_SHARED_LIBS=ON`으로 OBJ와 연결된 MTL을 읽는 범위만 포함했다.

`AEGIS.slnx`와 이 폴더를 같은 위치에 둔다. 프로젝트는 `$(SolutionDir)`로 헤더와 라이브러리 경로를 만들고, 실행 파일 옆으로 GLEW·Assimp DLL을 복사한다. 실습의 `glfw3.lib`에 맞춰 Debug와 Release에서 모두 DLL C 런타임(`/MD`)을 쓴다. FMOD는 [SDK 설치 안내](fmod/README.md)에 따라 사운드 개발 시 각자 준비한다.
