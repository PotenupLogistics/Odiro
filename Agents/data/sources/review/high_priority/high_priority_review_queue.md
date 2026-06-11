# High Priority Manual Review Queue

## 1. 목적

MVP 정책과 직접 연결되는 High priority 후보 43개를 원본 PDF와 수동 대조하기 위한 검토 문서이다.

## 2. 검토 원칙

* 이 문서는 policy card가 아니다.
* 모든 항목은 원본 PDF 대조 전이다.
* 원본 PDF에서 실제 문장을 확인한 뒤에만 confirmed 처리할 수 있다.
* confirmed 처리는 다음 단계에서 별도 작업으로 수행한다.
* 현재 상태는 모두 pending_manual_confirmation이다.

## 3. Source별 검토 순서

검토 권장 순서:

1. KOR-003 KIRIA 운행안전인증 가이드북
2. KOR-004 운행안전인증 절차 및 기준 고시
3. KOR-002 도로교통법
4. KOR-001 지능형로봇법
5. KOR-005 도로교통법 제2조 하위법령

## 4. High Priority 후보 목록

| No | sourceId | candidateId | category | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | manualReviewStatus |
| -- | -------- | ----------- | -------- | ------------- | ------------------ | --------------- | ------------------- | ------------------ |
| 1 | KOR-001 | CAND-KOR-001-010 | emergency_stop | 면의 점용ㆍ사용 실시계획의 승인 등(매립면허를 받은 매립예정지는 제외한다), 같은 법 제28조에 따른 공유수면 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 2 | KOR-002 | CAND-KOR-002-004 | speed_policy | 3. “고속도로”란 자동차의 고속 운행에만 사용하기 위하여 지정된 도로를 말한다. | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 3 | KOR-002 | CAND-KOR-002-005 | sidewalk_operation | 4. “차도”(車道)란 연석선(차도와 보도를 구분하는 돌 등으로 이어진 선을 말한다. 이하 같다), 안전표지 또는 그 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 4 | KOR-002 | CAND-KOR-002-010 | sidewalk_operation | 10. “보도”(步道)란 연석선, 안전표지나 그와 비슷한 인공구조물로 경계를 표시하여 보행자(유모차, 보행보조용 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 5 | KOR-002 | CAND-KOR-002-013 | sidewalk_operation | 11. “길가장자리구역”이란 보도와 차도가 구분되지 아니한 도로에서 보행자의 안전을 확보하기 위하여 안전표 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 6 | KOR-002 | CAND-KOR-002-014 | sidewalk_operation | 12. “횡단보도”란 보행자가 도로를 횡단할 수 있도록 안전표지로 표시한 도로의 부분을 말한다. | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 7 | KOR-002 | CAND-KOR-002-015 | sidewalk_operation | 13. “교차로”란 ‘십’자로, ‘T’자로나 그 밖에 둘 이상의 도로(보도와 차도가 구분되어 있는 도로에서는 차도를 말 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 8 | KOR-002 | CAND-KOR-002-016 | sidewalk_operation | 13의2. “회전교차로”란 교차로 중 차마가 원형의 교통섬(차마의 안전하고 원활한 교통처리나 보행자 도로횡단 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 9 | KOR-002 | CAND-KOR-002-018 | sidewalk_operation | 14. “안전지대”란 도로를 횡단하는 보행자나 통행하는 차마의 안전을 위하여 안전표지나 이와 비슷한 인공구조 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 10 | KOR-002 | CAND-KOR-002-019 | emergency_stop | 15. “신호기”란 도로교통에서 문자ㆍ기호 또는 등화(燈火)를 사용하여 진행ㆍ정지ㆍ방향전환ㆍ주의 등의 신호 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 11 | KOR-002 | CAND-KOR-002-023 | terrain_or_dynamic_safety | 18의2. “자율주행시스템”이란 「자율주행자동차 상용화 촉진 및 지원에 관한 법률」 제2조제1항제2호에 따른 자 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 12 | KOR-002 | CAND-KOR-002-024 | terrain_or_dynamic_safety | 율주행시스템을 말한다. 이 경우 그 종류는 완전 자율주행시스템, 부분 자율주행시스템 등 행정안전부령으로 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 13 | KOR-002 | CAND-KOR-002-025 | terrain_or_dynamic_safety | 18의3. “자율주행자동차”란 「자동차관리법」 제2조제1호의3에 따른 자율주행자동차로서 자율주행시스템을 갖 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 14 | KOR-002 | CAND-KOR-002-030 | emergency_stop | 24. “주차”란 운전자가 승객을 기다리거나 화물을 싣거나 차가 고장 나거나 그 밖의 사유로 차를 계속 정지 상 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 15 | KOR-002 | CAND-KOR-002-031 | emergency_stop | 25. “정차”란 운전자가 5분을 초과하지 아니하고 차를 정지시키는 것으로서 주차 외의 정지 상태를 말한다. | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 16 | KOR-002 | CAND-KOR-002-032 | terrain_or_dynamic_safety | 것(조종 또는 자율주행시스템을 사용하는 것을 포함한다)을 말한다. | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 17 | KOR-002 | CAND-KOR-002-033 | speed_policy | 28. “서행”(徐行)이란 운전자가 차 또는 노면전차를 즉시 정지시킬 수 있는 정도의 느린 속도로 진행하는 것을 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 18 | KOR-002 | CAND-KOR-002-034 | emergency_stop | 30. “일시정지”란 차 또는 노면전차의 운전자가 그 차 또는 노면전차의 바퀴를 일시적으로 완전히 정지시키는 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 19 | KOR-002 | CAND-KOR-002-035 | sidewalk_operation | 31. “보행자전용도로”란 보행자만 다닐 수 있도록 안전표지나 그와 비슷한 인공구조물로 표시한 도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 20 | KOR-002 | CAND-KOR-002-036 | sidewalk_operation | 31의2. “보행자우선도로”란 「보행안전 및 편의증진에 관한 법률」 제2조제3호에 따른 보행자우선도로를 말한다 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 21 | KOR-003 | CAND-KOR-003-026 | sidewalk_operation | 그림 5 신호등이 없는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·14 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 22 | KOR-003 | CAND-KOR-003-027 | sidewalk_operation | 그림 6 신호등이 있는 횡단보도 통행 시나리오 예시 · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · · ·15 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 23 | KOR-003 | CAND-KOR-003-050 | terrain_or_dynamic_safety | 1 질량 및 폭 제한 질량 폭 이하500 kg, 800 mm | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 24 | KOR-003 | CAND-KOR-003-051 | sidewalk_operation | * 단 보도 폭이 최소 이상이면 까지 허용2500 mm 1200 mm | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 25 | KOR-003 | CAND-KOR-003-052 | speed_policy | 2 운행 속도 질량별 최대 운행 속도 보호구역 운행 속도 준수, | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 26 | KOR-003 | CAND-KOR-003-053 | terrain_or_dynamic_safety | 4 동적 안정성 경사로 주행 및 구조물 통과 시 안정성 확인 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 27 | KOR-003 | CAND-KOR-003-054 | emergency_stop | 5 비상정지 비상정지 스위치 장착 및 요구사항 준수 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 28 | KOR-003 | CAND-KOR-003-056 | speed_policy | 7 속도 제어 설정된 운행 속도 준수 | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 29 | KOR-003 | CAND-KOR-003-057 | perception_requirement | 8 장애물 감지 관련 규격에 따른 장애물 형상 감지 및 회피 | PedestrianAhead, ObstacleAhead, ApproachingObject | SlowDown, Stop, RequestOperator | perceptionMinRangeM, pedestrianDetectionRequired | pending_manual_confirmation |
| 30 | KOR-003 | CAND-KOR-003-058 | terrain_or_dynamic_safety | 11 방수 성능 로봇 외함에 대한 기본 방수 성능 만족 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 31 | KOR-003 | CAND-KOR-003-059 | sidewalk_operation | 13 횡단보도 통행 횡단보도 통행 시 필수 요건 충족 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 32 | KOR-003 | CAND-KOR-003-060 | operator_control | 14 관제장치 모니터링 알림 등 관제장치 필수 요건 충족, | ApproachingObject | RequestOperator, Stop | operatorOverrideEnabled, maxRemoteResponseSec | pending_manual_confirmation |
| 33 | KOR-003 | CAND-KOR-003-062 | emergency_stop | 16 원격조작 원격에서 로봇을 정지시키는 수단 만족 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 34 | KOR-004 | CAND-KOR-004-010 | sidewalk_operation | 가.로봇의자체질량과적재물의질량을합하여500kg을초과하지않아야한다.나.로봇의폭은800mm를초과하지않아야한다.단,해당로봇이운행하려는보도의최소폭이2500mm이상인경우,로봇의폭을1200mm까지허용한다. | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 35 | KOR-004 | CAND-KOR-004-011 | speed_policy | 2.운행속도가.로봇의최대운행속도는최대질량(로봇과적재물질량의합)에따라,아래의기준에적합해야한다.최대질량(kg)최대운행속도(km/h)230초과500이하5100초과230이하10100이하 15나.가항에도불구하고,도로교통법제12조에따른어린이보호구역,제12조의2에따른노인보호구역또는장애인보호구역에서로봇이최대운행속도5km/h를준수하는지확인한다.3.겉모양가.로봇의표면에날카로운모서리와날카로운끝이없고,손가락끼임으로인해상해가발생하지않아야한다.나.접촉가능한날카로운부위의최소반경은3mm이상이어야한다. | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 36 | KOR-004 | CAND-KOR-004-012 | terrain_or_dynamic_safety | 가.로봇은그림과같이경사각(θ)이적어도5°이상인주행로에서시험하여안정적으로주행할수있어야한다.1)로봇을출발점(Po)에서목표점(Pt)까지주행하였을때,적재물의이탈이나낙하없이목표선을통과해야하며,통과지점(Pf)은허용범위(D)이내에있을것 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 37 | KOR-004 | CAND-KOR-004-013 | emergency_stop | 2)후진을하는로봇의경우,후진주행으로“1)”과동일한방법으로시험할것나.로봇은적어도높이30mm이상의구조물을전도,적재물의이탈이나낙하없이통과해야하며,평가방법은한국산업표준KSB7320의5.3.2에따른다.5.비상정지가.로봇은비정상적인작동이나위험한상황이발생한경우위험을즉 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 38 | KOR-004 | CAND-KOR-004-017 | terrain_or_dynamic_safety | 1)그림과같이로봇을출발지점에서도착지점으로자율주행하도록하였을때,로봇이금지구역을침범하지않을것2)출발지점A와B에대해각3회씩반복하여시험하였을때“1)”의조건을만족할것나.로봇이금지구역에진입하지아니하고회피하여주행하거나멈추는경우“가”에만족한것으로간주한다. | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM | pending_manual_confirmation |
| 39 | KOR-004 | CAND-KOR-004-018 | speed_policy | 가.로봇은경사가없는평지에서설정된속도에대하여정확도±10%이내의속도로주행해야한다.1)로봇의속도를측정하는구간은1m이상일것2)로봇은속도를측정하는구간에이르기전에충분한가속이이루어질것3)로봇의속도는고유사양에서명시된최댓값으로설정할것4)시험은총3회수행하여그결과를평균한값이“가”를만족할것나.로봇은적어도5°이상의경사가있는주행로에서설정된속도에대하여정확도±10%이내의속도로주행해야한다.1)“가”의방법과동일한조건으로시험할것2)경사각은고유사양에서명시된최대주행가능경사각으로설정할것3)오르막과내리막에대하여동일한방법으로시험할것8.장애물감지가.로봇은사람이나장애물을감지하여... | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh | pending_manual_confirmation |
| 40 | KOR-004 | CAND-KOR-004-019 | perception_requirement | 나.장애물배치방법은그림과같다.단,폭이800mm를초과하는로봇의경우초과하는폭의길이만큼시험장의폭을확장시킨다. | PedestrianAhead, ObstacleAhead, ApproachingObject | SlowDown, Stop, RequestOperator | perceptionMinRangeM, pedestrianDetectionRequired | pending_manual_confirmation |
| 41 | KOR-004 | CAND-KOR-004-020 | emergency_stop | ※이때α(mm)의값은로봇폭(mm)–800(mm)이다.9.알림음가.로봇은통행요청,고장상태등안전한운행에필요한적절한알림음을발생해야한다.나.로봇의알림음중발화음과비상정지알림음은55dB(A)이상,73dB(A)이하이어야하며,평가방법은한국산업표준KSB7320의5.7절(알림음)에따른다.10.등화장치가.로봇은시각적알림을제공하기위한등화장치를가져야한다.나.등화장치는한국산업표준KSB7320에따른시험환경에서표면온도가섭씨60도를초과하지않아야한다.온도의측정은등화장치를30분이상작동상태로유지한이후시험해야한다.11.방수성능가.로봇의외함은한국산업표준KSCIEC6052... | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM | pending_manual_confirmation |
| 42 | KOR-004 | CAND-KOR-004-021 | sidewalk_operation | 13.횡단보도통행가.로봇이횡단보도를통행하는경우에는아래의요구사항을모두만족하도록해야한다.1)신호를대기하는로봇은운행계획서에명시된보도상의구역에위치할것2)로봇은신호등의신호를정확히인지할수있는수단을가질것3)로봇이횡단보도통행을위해대기장소에도착한시점에보행신호가작동중인경우에는,정지상태로대기후다음보행신호에횡단할것4)횡단중보행자및차량등장애물과의접촉이없을것5)로봇은보행신호가종료되기이전에횡단을완료할것 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
| 43 | KOR-004 | CAND-KOR-004-022 | sidewalk_operation | 14.관제장치가.로봇의관제장치는운용중인모든로봇을식별해야한다.나.관제장치는모든로봇에대하여다음의상태에대한정보를실시간으로확인할수있어야한다.1)로봇의위치와속력2)로봇의주행가능거리또는배터리잔량3)로봇과관제장치간통신신호의감도4)로봇의상태(주행,대기,비상정지,오류등)다.로봇은주행하는경로에보행자밀집등으로인해통행이불가능한 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired | pending_manual_confirmation |
