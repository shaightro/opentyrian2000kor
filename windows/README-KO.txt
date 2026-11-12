OpenTyrian2000 한국어판 (개발 중)
================================================================================

Tyrian 2000을 요즘 Windows에서 실행하는 오픈소스 이식판 OpenTyrian2000을
한국어로 옮기는 포크입니다.

게임 안의 글(메뉴, 도움말, 아이템, 브리핑, 데이터 큐브)을 한국어로 옮겼습니다.
아직 실제 플레이로 모든 화면을 확인하지는 못했으니, 어색하거나 넘치는 글이
있으면 알려 주세요.


== 실행 ========================================================================

OpenTyrian2000.exe 를 더블클릭하세요.

게임 데이터(Tyrian 2000 프리웨어)는 data 폴더에 들어 있습니다. data 폴더의
.exe 파일들은 원래 게임(DOS/Windows 95)의 실행 파일이라 쓰지 않습니다.

data 폴더가 없으면 처음 실행할 때 공식 배포처 camanis.net에서
tyrian2000.zip(약 5MB)을 내려받을지 묻습니다. 인터넷을 쓸 수 없다면 아래 주소의
tyrian2000.zip 을 다른 곳에서 받아 이 폴더(OpenTyrian2000.exe 옆)에 두고
실행하세요. 압축은 자동으로 풀립니다.

  https://www.camanis.net/tyrian/tyrian2000.zip


== 요구 사항 ===================================================================

Windows 10(1903 이상) 또는 Windows 11, 64비트


== 조작 ========================================================================

방향키          이동
스페이스        무기 발사
엔터            후방 무기 모드 전환
Ctrl / Alt      왼쪽 / 오른쪽 사이드킥 발사
Alt+Enter       전체 화면 전환


== 화면 ========================================================================

창 크기는 모니터에 맞춰 자동으로 정해지며(원래 화면 320x200의 정수배),
설정 > 그래픽 메뉴에서 바꿀 수 있습니다.

  화면           창 모드 또는 1번 모니터, 2번 모니터...(그 모니터의 전체 화면)
  창 크기        창 모드의 크기. 자동 또는 1x부터 모니터에 들어가는 최대 배수까지
  확대 방식      픽셀 확대 방식 (None = 각진 픽셀 그대로, Scale2x/hq2x 등 = 윤곽을 다듬음)
  화면 맞춤      정수배 = 정수배로만 확대(가장 선명함)
                 8:5 맞춤 = 화면에 꽉 차게(비율 유지)
                 4:3 맞춤 = 옛 CRT 모니터처럼 세로로 조금 길게

전체 화면은 화면 비율을 유지한 채 남는 부분을 검은 여백으로 둡니다.
윈도우 디스플레이 배율(125%, 150% 등)과 관계없이 모니터의 실제 픽셀로
선명하게 그립니다.

마우스: 게임 플레이 중에는 마우스 포인터가 창 밖으로 나가지 않습니다.
전체 화면에서는 메뉴에서도 포인터가 게임 화면 안에 머뭅니다.


== 파일 위치 ===================================================================

설정과 저장 파일(opentyrian.cfg, tyrian.cfg, tyrian.sav)은 이 폴더에 저장됩니다.
이 폴더에 쓸 수 없는 경우(예: Program Files 아래)에는 아래 위치에 저장됩니다.

  %APPDATA%\OpenTyrian\OpenTyrian2000


== 저작권과 배포 조건 ==========================================================

이 배포판은 무료이며, 비상업적인 용도로만 배포합니다. 판매하거나 유료로
제공하는 상품이나 서비스에 넣지 마세요.

Tyrian 2000 게임 데이터 - data 폴더
  Tyrian 2000 Game Copyright (c) 1999, Eclipse Software
  Tyrian 2000 Copyright (c) 1999, Stealth Productions, Inc.
  만든 사람: Jason Emery(프로그래밍), Alexander Brandon(이야기, 음악),
  Daniel Cook(그래픽) 외 (전체 제작진은 게임의 제작진 목록에 있습니다)

  원저작자가 프리웨어로 공개한 파일(위 camanis.net 주소의 tyrian2000.zip)을
  고치지 않고 그대로 넣었습니다. 저작권은 원저작권자에게 있으며, 원본 안내문은
  data\readme.txt 입니다. 권리자가 이의를 제기하면 게임 데이터 배포를 바로
  중단하겠습니다. 연락: https://github.com/shaightro/opentyrian2000kor/issues

OpenTyrian2000 - 이 프로그램
  OpenTyrian: The OpenTyrian Development Team
  OpenTyrian2000: Kaito Sinclaire, https://github.com/KScl/opentyrian2000
  한국어판: https://github.com/shaightro/opentyrian2000kor
  GNU General Public License 버전 2 이상 (COPYING.txt)

SDL2
  zlib 라이선스 (SDL2-LICENSE.txt)

갈무리(Galmuri) 글꼴 - kor 폴더의 Galmuri11-Bold.ttf, Galmuri9.ttf
  SIL Open Font License 1.1 (kor\Galmuri-OFL.md)
  Copyright 2019-2025 Lee Minseo, https://github.com/quiple/galmuri
