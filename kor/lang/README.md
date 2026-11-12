# 번역 안내

이 폴더의 `.po` 파일들이 OpenTyrian2000 한국어판의 번역입니다. 게임은 실행할 때
여기서 번역을 읽고, 번역이 없는 문장은 영어 그대로 보여 줍니다.

## 파일

| 파일 | 내용 |
|---|---|
| `help.po` | 메뉴, 도움말, 함선 설명 등 대부분의 화면 문구 (`tyrian.hdt`) |
| `items.po` | 무기·함선·보호막 등 아이템 이름 |
| `levels.po` | 레벨 이름 |
| `episode1.po` ~ `episode5.po` | 레벨 전 브리핑과 레벨 후 힌트 |
| `cubes1.po` ~ `cubes5.po` | 데이터 큐브 (분량의 대부분) |
| `credits.po` | 제작진 목록 |
| `ui.po` | 이 판에서 추가한 설정 메뉴 등 |

## 형식

항목 하나는 이렇게 생겼습니다.

```
#. 게임 메뉴 항목 (보통 글씨). 최대 17바이트 (한글 약 5자)
msgctxt "help.menuInt.1.2"
msgid "Ship Specs"
msgstr "함선 정보"
```

- `msgstr`에만 번역을 씁니다. `msgctxt`(게임이 문장을 찾는 열쇠)와 `msgid`(영어 원문)는 바꾸지 않습니다.
- `msgstr`을 비워 두면 영어가 나옵니다.
- `#.` 줄은 그 문장이 나오는 곳과 길이 제한입니다. 한글 한 글자는 3바이트입니다.
- `~`로 감싼 부분은 더 밝게 강조됩니다. 번역에서도 강조할 낱말을 `~`로 감싸세요.
- `%d`는 숫자가 들어갈 자리입니다. 그대로 두세요.
- 따옴표는 `\"`로 씁니다.
- 브리핑과 힌트(`episode*.po`)는 `\n`으로 줄을 나눕니다. 화면에 그 줄 그대로 나오며, 줄 수는 바꿔도 됩니다. 한 줄은 작은 글씨로 화면 폭(약 300픽셀, 한글 40자 남짓) 안에 들어가야 합니다.
- 데이터 큐브(`cubes*.po`)는 문단 하나가 항목 하나입니다. 줄바꿈은 게임이 알아서 합니다.
- 제작진 목록(`credits.po`)의 첫 글자는 그 줄의 색을 정하는 기호입니다. 그대로 두세요.
- 다시 봐야 할 번역은 항목 위에 `#, fuzzy` 줄을 두면 게임이 쓰지 않습니다.
- 바이트 제한은 게임 안의 칸 크기라서 넘으면 잘립니다. 하지만 대개는 화면 폭이 먼저 차므로, 메뉴 항목처럼 짧은 문장은 영어와 비슷한 길이로 짓는 것이 안전합니다.

## 다른 AI에게 맡길 때

파일 하나를 통째로 주고, 아래 지시와 용어집을 함께 주면 됩니다. 받은 결과로 파일을
덮어쓴 뒤 아래 "검사"를 돌려 보세요.

> 이 파일은 1999년 DOS 슈팅 게임 Tyrian 2000의 번역 파일(gettext PO)이다.
> 각 항목의 msgstr에 자연스러운 한국어 번역을 채워라. 나머지 줄(msgctxt, msgid,
> #. 주석)은 한 글자도 바꾸지 말고 파일 전체를 그대로 돌려줘라.
> `#.` 주석의 길이 제한을 지키고, `~`, `%d`, `\n`, `\"`는 원문과 같은 방식으로 써라.
> SF 게임의 분위기를 살리되, 인물 대사의 말투는 아래 "말투" 규칙을 따르라.
> 고유명사는 아래 용어집을 따르라.

## 말투

배경: 20,031년, 전투기 조종사 트렌트 호킨스가 거대 기업 마이크로솔에 맞서 싸운다.
젠코어는 트렌트를 돕는 기술 동맹이고, 징글론 교단은 적대 세력이다. 진지한 SF와
엉뚱한 농담(과일 금지령, 음식 함선, 프레첼 등)이 섞인 게임이므로 농담은 살려서
자연스러운 한국어 말장난으로 옮긴다.

| 글의 종류 | 말투 | 예 |
|---|---|---|
| 메뉴 항목, 화면 이름 | 짧은 명사형 | 함선 정보, 게임 종료 |
| 메뉴 설명, 도움말, 안내 | 합니다체 | 새 무기와 함선 부품을 삽니다. |
| 시스템 경고, 브리핑(레벨 전 화면) | 짧은 보고체(합니다체) | 전방에서 정체불명의 자기력이 감지되었습니다. |
| 비밀 힌트 | 합니다체 | 첫 U자 함선에는 비밀이 있습니다. |
| 젠코어의 공식 임무 지시(보안 책임자 등) | 명령조 하라체 | 먼저 토름으로 항로를 잡고 조사하라. |
| 트랜슨 로크(젠코어 의장, 노인)가 트렌트에게 | 하게체 | 사바라로 가서 조사해 주게. |
| 가족, 친구, 동료가 트렌트에게 | 반말 | 아빠가 너를 정말 자랑스러워한단다. |
| 행성 통치자, 공식 비상 통신 | 하십시오체 | 이것은 훈련이 아닙니다! |
| 하주드라(정중하지만 조금 어색한 외계인) | 정중한 합니다체 | 다시 한번 사과드립니다. |
| 징글론 교단(적, 거만하고 고풍스러운) | 위압적인 반말, 고어투 | 너는 또다시 우리의 길을 더럽히는구나. |
| 뉴스, 기록 보관소 자료, 관광 안내서 | 기록문체(~다) | 사바라는 바다로 덮인 행성이다. |
| 광고, 상점 | 친근한 광고체 | 지금 바로 주문하세요! |
| 개인 일지 | 독백체(~다) | 오늘도 아무 일 없었다. |

한 인물의 말투는 모든 파일에서 같게 유지한다.

## 용어집

### 인물, 집단

| 영어 | 한국어 | 비고 |
|---|---|---|
| Trent Hawkins | 트렌트 호킨스 | 주인공 |
| Transon Lohk | 트랜슨 로크 | 젠코어 의장 |
| Emperor Milktoe | 밀크토 황제 | 사바라 통치자 |
| Javi | 하비 | |
| Zinglon | 징글론 | 교단이 섬기는 신 |
| Zinglonite(s) | 징글론 신도 | |
| Order of Zinglon | 징글론 교단 | |
| Grand Moolah Yimbo | 대 물라 임보 | |
| Hazudra | 하주드라 | 외계 종족 |
| Pretzel Pete | 프레첼 피트 | |
| Dai-Zan | 다이잔 | |
| Zik-Nar | 직나르 | |
| Vykromod | 바이크로모드 | |
| Steffan Tommas | 스테판 토마스 | |
| Nortaneous | 노르타니어스 | |
| Pib | 피브 | 고양이, 자신을 "짐"이라 부름 |
| Javi Onukala | 하비 오누칼라 | 젠코어 보안 책임자. 해라체, 배신 전에는 트렌트를 "귀관", 뒤에는 "너" |
| Dougan | 더건 | 트렌트의 친구, 반말 ("두건"과 헷갈리지 않게) |
| Reid | 리드 | 사바라 반군 지도자, 뒤에 대통령 권한 대행. 하십시오체 |
| Mondus | 몬더스 | 델리아니 관제부 수장. 하십시오체, "트랜슨 의장님" |
| Harble Wom | 하블 웜 | 다운스타 요원. 트렌트에게 해요체, 상부 보고는 합니다체 |
| Ninja | 닌자 | 수수께끼의 인물, 트렌트에게 반말 |
| Captain Nob | 노브 함장 | 적대적인 반말 |
| Rob Neither | 롭 니더 | 뉴스 진행자 ("오늘의 뉴스, 롭 니더입니다.") |
| Prime Chair | 회장 | 마이크로솔의 수장 (젠코어의 "의장"과 구분). Durril 더릴 |
| Beppo / Borogar / Nanol | 베포 / 보로가르 / 나놀 | |
| Almos / Almo | 알모스 / 알모 | 트랜슨의 부하 / 기생충 강아지 |
| Muldar | 멀다르 | |
| Monses | 몬세 | 모세 패러디 |
| Gyges Phildren | 기게스 필드런 | |
| Hazudra Collector | 하주드라 수집가 | |
| Buce | 부스 | 1편의 하주드라 친구 |
| Baker Bob | 제빵사 밥 | |
| ShipMaster Carlos | 카를로스 선장 | |

### 기업, 조직, 매체

| 영어 | 한국어 |
|---|---|
| Microsol / MicroSol | 마이크로솔 |
| MicroCorp | 마이크로코프 |
| Gencore | 젠코어 |
| Gencore Command Ship IcoStar II | 젠코어 지휘함 아이코스타 II |
| Zica | 지카 |
| Beno | 베노 |
| Fluxnet | 플럭스넷 |
| Holo News | 홀로 뉴스 |
| Eclipse Central | 이클립스 센트럴 |
| XSIV Games | XSIV 게임즈 |
| MagVend | 매그벤드 |
| Omara corp. / Beno Crackers | 오마라 사 / 베노 크래커 |
| Brownie Patrol | 브라우니 순찰대 |
| Fluxnet 21 | 플럭스넷 21 |
| Magnatower Center | 매그나타워 센터 |

### 행성, 장소

| 영어 | 한국어 |
|---|---|
| Tyrian | 티리안 (게임 제목도 "티리안") |
| Savara | 사바라 |
| Midway | 미드웨이 (사바라의 도시) |
| Tonchus | 톤추스 (사바라의 도시) |
| Deliani | 델리아니 |
| Torm | 토름 |
| Gyges | 기게스 |
| Ixmucane | 익스무카네 |
| Camanis | 카마니스 |
| Station Gryphon | 그리폰 정거장 |
| Stargate | 스타게이트 |
| Asteroid City | 소행성 도시 |
| Asteroid Field | 소행성 지대 |
| Soh Jin | 소진 |
| Fauchi mines | 파우치 광산 |
| Downstar | 다운스타 |

### 게임 용어

| 영어 | 한국어 |
|---|---|
| Data Cube / holocube | 데이터 큐브 / 홀로큐브 |
| dataflux / flux | 데이터 흐름 / 흐름 |
| transmission | 통신 |
| Front Gun / Rear Gun | 전방 무기 / 후방 무기 |
| Sidekick | 사이드킥 |
| Generator | 발전기 |
| Shield | 보호막 |
| Armor | 장갑 |
| credits (cr) | 크레딧 |
| Gravitic Ore | 중력 광석 |
| Super Arcade | 슈퍼 아케이드 |
| Timed Battle | 시간제 전투 |
| Fokar-Cycle | 포카 주기 |
| sector / (star) system / section | 섹터 / 성계 / 구역 |
| Gravitium | 그래비튬 (중력 광석과 다른 물질) |
| Lightning (무기) | 번개 (번개 캐논, 번개 블래스트) |
| tightbeam | 지향성 빔 |
| rubnik (화폐) | 루브닉 |
| Gruel / Interstellar House of Gruel | 죽 / 성간 죽 하우스 |
| Zinglon's Ale | 징글론의 에일 |
| Skip It (다음 레벨 메뉴) | 건너뛰기 |
| Remus / Mendivian | 레무스 / 멘디비안 |
| Deliani Control | 델리아니 관제부 |

레벨 이름은 `levels.po`를 따른다 (예: GAUNTLET 시련의 길, HOLES 구멍, ASTEROID1 소행성 1).

### 데이터 큐브의 상투구

| 영어 | 한국어 |
|---|---|
| INCOMING TRANSMISSION from X | X발 수신 통신 (아이코스타는 "젠코어 지휘함 아이코스타 II에서 온 통신") |
| TRANSMISSION SOURCE: X | 통신 출처: X |
| PRIORITY UPLINK A1 | 우선 업링크 A1 |
| ARCHIVE DATA / Reference: X / See X | 기록 보관소 자료 / 항목: X / 참조: X |
| Rated: Not Funny | 평가: 안 웃김 |
| Scenic guide to the universe: X | 우주 관광 안내서: X (먹거리 / 문화 / 즐길 거리) |
| Tightbeam private coded message. | 지향성 빔 개인 암호 메시지. |
| "Transon out." | "이상, 트랜슨." |
| [Transmission End] / [End of File] | [통신 종료] / [파일 끝] |

### 아이템 이름 짓기

- 상표·제품 이름은 소리대로 적는다: Zica 지카, Protron 프로트론, Vulcan 벌컨, NortShip 노트쉽, Gencore Phoenix 젠코어 피닉스, USP Talon 탤런.
- 무기 종류는 굳어진 외래어로: Cannon 캐논, Laser 레이저, Missile 미사일, Launcher 발사기, Bomb 폭탄, Beam 빔, Wave 웨이브, Pulse 펄스, Blaster 블래스터.
- 뜻이 분명한 꾸밈말은 번역한다: Standard 표준, Advanced 고급, Heavy 대형, Guided 유도, Rear 후방, Dual 이중, Light Fighter 경전투기, Ammo 탄약, Option 옵션.
- 우스운 이름은 웃음이 살게 옮긴다: Banana Blast 바나나 블래스트, The Orange Juicer 오렌지 착즙기.

번역하면서 새 고유명사를 정하면 여기에 더해 주세요.

## 검사와 다시 추출

게임 데이터가 있는 상태에서 실행 파일에 옵션을 붙여 실행합니다.

```
OpenTyrian2000.exe --check-text
```

번역을 검사해서 결과를 이 폴더의 `check-report.txt`에 씁니다.

- 오류: 형식 오류, 게임에 없는 열쇠, 칸(바이트) 초과, `%d`·`%s` 불일치, 제작진 목록의 색 기호 변경, 글꼴에 없는 글자
- 경고: `~` 짝 불일치, 자리를 아는 문장(메뉴 제목과 항목, 화면 아래 도움말, 브리핑 줄, 레벨 이름, 큐브 제목)의 화면 폭 초과, 한쪽 글꼴에만 있는 글자
- 참고: 그 밖의 짧은 문장이 영어보다 꽤 넓을 때 (화면에서 직접 확인해 보세요)

```
OpenTyrian2000.exe --preview-text
```

플레이해야 볼 수 있는 화면(모든 브리핑과 비밀 힌트, 제작진 목록, 데이터 큐브 목록과
큐브의 모든 쪽)을 게임의 그리기 코드로 그려서, 실행 파일 옆 `text-preview` 폴더에
BMP 그림으로 저장합니다. `index.txt`에 그림마다 해당 열쇠가 적혀 있습니다.

```
OpenTyrian2000.exe --dump-text
```

게임의 영어 문장을 다시 뽑아 이 폴더의 `.po` 파일을 새로 씁니다. 이미 있는 번역은
열쇠가 같으면 그대로 유지됩니다. 결과는 `dump-report.txt`에 남습니다.

배포 패키지에는 영어 원문을 뺀, 번역된 항목만 들어갑니다 (`windows/build.ps1`).
