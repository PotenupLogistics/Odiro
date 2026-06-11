# KOR-004 Manual Review Pack

## 1. 검토 대상 문서

* sourceId: KOR-004
* sourceTitle: 실외이동로봇 운행안전인증 절차 및 기준 등에 관한 고시
* rawPdfPath: data/sources/raw/korea/KOR-004_산업통상자원부_운행안전인증_절차_및_기준_고시.pdf
* highPriorityCount: 10
* pageHintFoundCount: 8
* pageHintPartialCount: 2

## 2. 검토 방법

1. rawPdfPath의 원본 PDF를 연다.
2. 아래 후보의 page hint 페이지를 확인한다.
3. extractedText가 원본 PDF에 실제로 존재하는지 확인한다.
4. 정책으로 사용할 수 있는 문장인지 판단한다.
5. manual_confirmation_results.json에 사람이 직접 다음 필드를 입력한다.

   * manualReviewStatus: confirmed 또는 rejected
   * rawPdfPage
   * rawPdfSection
   * confirmedText
   * reviewer
   * reviewedAt
   * decisionReason
   * nextAction

## 3. 후보 목록

| No | candidateId | category | hintStatus | pageHints | extractedText | linkedMvpSituation | linkedMvpAction | relatedPolicyParams | 검토 메모 |
| -- | ----------- | -------- | ---------- | --------- | ------------- | ------------------ | --------------- | ------------------- | ----- |
| 1 | CAND-KOR-004-010 | sidewalk_operation | found | 8 | 가.로봇의자체질량과적재물의질량을합하여500kg을초과하지않아야한다.나.로봇의폭은800mm를초과하지않아야한다.단,해당로봇이운행하려는보도의최소폭이2500mm이상인경우,로봇의폭을1200mm까지허용한다. | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 2 | CAND-KOR-004-011 | speed_policy | found | 8 | 2.운행속도가.로봇의최대운행속도는최대질량(로봇과적재물질량의합)에따라,아래의기준에적합해야한다.최대질량(kg)최대운행속도(km/h)230초과500이하5100초과230이하10100이하 15나.가항에도불구하고,도로교통법제12조에따른어린이보호구역,제12조의2에따른노인보호구역또는장애인보호구역에서로봇이최대운행속도5km/h를준수하는지확인한다.3.겉모양가.로봇의표면에날카로운모서리와날카로운끝이없고,손가락끼임으로인해상해가발생하지않아야한다.나.접촉가능한날카로운부위의최소반경은3mm이상이어야한다. | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 3 | CAND-KOR-004-012 | terrain_or_dynamic_safety | found | 8 | 가.로봇은그림과같이경사각(θ)이적어도5°이상인주행로에서시험하여안정적으로주행할수있어야한다.1)로봇을출발점(Po)에서목표점(Pt)까지주행하였을때,적재물의이탈이나낙하없이목표선을통과해야하며,통과지점(Pf)은허용범위(D)이내에있을것 | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 4 | CAND-KOR-004-013 | emergency_stop | found | 8 | 2)후진을하는로봇의경우,후진주행으로“1)”과동일한방법으로시험할것나.로봇은적어도높이30mm이상의구조물을전도,적재물의이탈이나낙하없이통과해야하며,평가방법은한국산업표준KSB7320의5.3.2에따른다.5.비상정지가.로봇은비정상적인작동이나위험한상황이발생한경우위험을즉 | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 5 | CAND-KOR-004-017 | terrain_or_dynamic_safety | found | 9 | 1)그림과같이로봇을출발지점에서도착지점으로자율주행하도록하였을때,로봇이금지구역을침범하지않을것2)출발지점A와B에대해각3회씩반복하여시험하였을때“1)”의조건을만족할것나.로봇이금지구역에진입하지아니하고회피하여주행하거나멈추는경우“가”에만족한것으로간주한다. | FallOrTilt, TerrainRisk, ObstacleAhead | ReplanPath, LocalAvoidance, Stop | maxSlopeDeg, obstacleClearanceM |  |
| 6 | CAND-KOR-004-018 | speed_policy | partial | 9 | 가.로봇은경사가없는평지에서설정된속도에대하여정확도±10%이내의속도로주행해야한다.1)로봇의속도를측정하는구간은1m이상일것2)로봇은속도를측정하는구간에이르기전에충분한가속이이루어질것3)로봇의속도는고유사양에서명시된최댓값으로설정할것4)시험은총3회수행하여그결과를평균한값이“가”를만족할것나.로봇은적어도5°이상의경사가있는주행로에서설정된속도에대하여정확도±10%이내의속도로주행해야한다.1)“가”의방법과동일한조건으로시험할것2)경사각은고유사양에서명시된최대주행가능경사각으로설정할것3)오르막과내리막에대하여동일한방법으로시험할것8.장애물감지가.로봇은사람이나장애물을감지하여... | PedestrianAhead | SlowDown, Stop | maxSpeedKmh, lowSpeedZoneSpeedKmh |  |
| 7 | CAND-KOR-004-019 | perception_requirement | found | 10 | 나.장애물배치방법은그림과같다.단,폭이800mm를초과하는로봇의경우초과하는폭의길이만큼시험장의폭을확장시킨다. | PedestrianAhead, ObstacleAhead, ApproachingObject | SlowDown, Stop, RequestOperator | perceptionMinRangeM, pedestrianDetectionRequired |  |
| 8 | CAND-KOR-004-020 | emergency_stop | partial | 10 | ※이때α(mm)의값은로봇폭(mm)–800(mm)이다.9.알림음가.로봇은통행요청,고장상태등안전한운행에필요한적절한알림음을발생해야한다.나.로봇의알림음중발화음과비상정지알림음은55dB(A)이상,73dB(A)이하이어야하며,평가방법은한국산업표준KSB7320의5.7절(알림음)에따른다.10.등화장치가.로봇은시각적알림을제공하기위한등화장치를가져야한다.나.등화장치는한국산업표준KSB7320에따른시험환경에서표면온도가섭씨60도를초과하지않아야한다.온도의측정은등화장치를30분이상작동상태로유지한이후시험해야한다.11.방수성능가.로봇의외함은한국산업표준KSCIEC6052... | ObstacleAhead | EmergencyStop, Stop | emergencyStopEnabled, minStopDistanceM |  |
| 9 | CAND-KOR-004-021 | sidewalk_operation | found | 10 | 13.횡단보도통행가.로봇이횡단보도를통행하는경우에는아래의요구사항을모두만족하도록해야한다.1)신호를대기하는로봇은운행계획서에명시된보도상의구역에위치할것2)로봇은신호등의신호를정확히인지할수있는수단을가질것3)로봇이횡단보도통행을위해대기장소에도착한시점에보행신호가작동중인경우에는,정지상태로대기후다음보행신호에횡단할것4)횡단중보행자및차량등장애물과의접촉이없을것5)로봇은보행신호가종료되기이전에횡단을완료할것 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |
| 10 | CAND-KOR-004-022 | sidewalk_operation | found | 10 | 14.관제장치가.로봇의관제장치는운용중인모든로봇을식별해야한다.나.관제장치는모든로봇에대하여다음의상태에대한정보를실시간으로확인할수있어야한다.1)로봇의위치와속력2)로봇의주행가능거리또는배터리잔량3)로봇과관제장치간통신신호의감도4)로봇의상태(주행,대기,비상정지,오류등)다.로봇은주행하는경로에보행자밀집등으로인해통행이불가능한 | PedestrianAhead | YieldWait, SlowDown, Continue | sidewalkAllowed, pedestrianPriorityRequired |  |

주의:

* pageHints는 확정 근거가 아니라 검토 힌트이다.
* confirmed/rejected 판단은 사람이 수행한다.
* extractedText가 긴 경우 300자 이내로만 표시한다.
* 원문을 길게 복사하지 않는다.
