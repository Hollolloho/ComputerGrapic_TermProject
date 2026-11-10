# FMOD SDK 설치 위치

사운드 개발을 시작할 때 각자 [FMOD 공식 다운로드](https://www.fmod.com/download)에 로그인해 같은 Windows용 FMOD Engine SDK 버전을 받는다. 설치한 SDK의 `api/` 폴더를 이 폴더에 두면 Core 헤더는 `external/fmod/api/core/inc/`, x64 라이브러리는 `external/fmod/api/core/lib/x64/`에서 찾을 수 있다.

[FMOD SDK 배포 조건](https://www.fmod.com/legal)에 따라 SDK 파일은 각자 설치한다. 이 폴더는 설치 안내만 Git에서 관리하며, 현재 게임 대상은 FMOD 설치 없이 빌드된다.
