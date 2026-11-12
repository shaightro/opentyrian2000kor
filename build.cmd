@echo off
chcp 65001 >nul
rem 탐색기에서 더블클릭하면 Windows 배포 패키지를 만듭니다 (windows\build.ps1 실행).
rem 결과: dist\OpenTyrian2000-KR\ 폴더와 dist\OpenTyrian2000-KR-버전-win64.zip
rem 필요한 것: Visual Studio의 "C++를 사용한 데스크톱 개발" 워크로드.
rem SDL2와 게임 데이터(tyrian2000.zip)는 저장소 옆 폴더에 없으면 내려받습니다.
rem Git이 있으면 버전에 커밋 번호가 붙습니다.
rem 옵션은 그대로 넘깁니다. 예: build.cmd -NoZip, build.cmd -NoData (게임 데이터 빼기)
setlocal
cd /d "%~dp0"

echo OpenTyrian2000 한국어판을 빌드합니다...
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0windows\build.ps1" %*
set "result=%errorlevel%"

echo.
if "%result%"=="0" goto done
echo 빌드에 실패했습니다. 위의 메시지를 확인하세요.
goto finish

:done
echo 빌드가 끝났습니다. 결과는 dist 폴더에 있습니다.
if not defined BUILD_NO_PAUSE start "" explorer "%~dp0dist"

:finish
if not defined BUILD_NO_PAUSE pause
exit /b %result%
