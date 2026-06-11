# RSR-001 METRANS Sidewalk ADR Interactions

## 1. Source Metadata

* sourceId: RSR-001
* title: Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists
* originalFilePath: data/sources/raw/research/RSR-001_METRANS_Sidewalk_ADR_Interactions.pdf
* processedAt: 2026-05-30T14:16:58+00:00
* extractionStatus: partial
* extractionMethod: pypdf PdfReader.extract_text
* notes: Text was extracted, but tables, figures, page references, and layout require manual PDF review.

## 2. 문서 개요

PDF 추출 텍스트에서 sidewalk autonomous delivery robot, pedestrian, bicyclist, interaction, PET, conflict 키워드가 확인됩니다. 세부 의미와 페이지 근거는 원본 PDF 대조가 필요합니다.

## 3. 프로젝트 관련 키워드

* sidewalk autonomous delivery robot
* pedestrian
* bicyclist
* interaction
* PET
* conflict
* near-miss
* near miss
* safety
* observation
* delivery robot

## 4. 정책/실험 설계와 연결될 수 있는 내용

| 항목 | 문서에서 확인된 내용 요약 | 연결 가능 정책/실험 요소 | 확인 상태 |
| -- | -------------- | -------------- | ----- |
| pedestrian_interaction | Evaluation of Sidewalk  Autonomous Delivery  Robot Interactions  with Pedestrians and  Bicyclists  August 2022 A Research Report from the Pacific Southwest  Region University Transportation Center    Steven R. Gehrke, Northern Arizona Unive | pedestrian_interaction | 검토 필요 |
| near_miss_metric | hich  may reflect discomfort experienced by a survey respondent from a previous near-miss or collision with  an SADR operating in a real-world setting and concern over a similar future occurrence.   Evaluation of Sidewalk Autonomous Delivery Robot I | near_miss_metric | 검토 필요 |
| pet_metric | determined by adapting the surrogate safety measure of post-encroachment time  (PET) to account for pathway user movements and trajectories at sites with ambiguity regarding travel  lanes. The locations of observed SADR-involved interactions w | pet_metric | 검토 필요 |
| delivery_robot_scenario | ik, Northern Arizona University               Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists    2    TECHNICAL REPORT DOCUMENTATION PAGE  1. Report No.  PSR-21-16  2. Government Accession No.  N/A  3. Recipi | delivery_robot_scenario | 검토 필요 |
| comfort_and_safety | —comprising two studies—offers  evidence on the objective safety and perceived comfort experienced by pedestrians and bicyclists interacting with SADRs on  multi-use paths. In the first study, SADR interactions with human pathway users observed v | comfort_and_safety | 검토 필요 |
| scenario_design | results illustrate that survey respondents tended to be comfortable in  either scenario but were more likely to report a neutral comfort level after viewing the video with the  lower evasive maneuver time. The distribution of responses following t | scenario_design | 검토 필요 |
| evaluation_metric | including a semi-automated adaptation of the traditional vehicle-involved PET  metric based on walking trajectories. However, the authors noted their effort to adapt PET  measurements to smaller shared spaces may be insufficient alone for determ | evaluation_metric | 검토 필요 |

## 5. 추후 정책/실험 카드 후보

- sourceId: RSR-001 / 후보: pedestrian_interaction / 상태: 검토 필요
- sourceId: RSR-001 / 후보: near_miss_metric / 상태: 검토 필요
- sourceId: RSR-001 / 후보: pet_metric / 상태: 검토 필요
- sourceId: RSR-001 / 후보: delivery_robot_scenario / 상태: 검토 필요
- sourceId: RSR-001 / 후보: comfort_and_safety / 상태: 검토 필요
- sourceId: RSR-001 / 후보: scenario_design / 상태: 검토 필요
- sourceId: RSR-001 / 후보: evaluation_metric / 상태: 검토 필요

## 6. 수동 검토 필요 사항

* PDF page count from extractor: 54
* 표, 이미지, 수식, 알고리즘 구조는 원본 PDF 대조 필요
* 페이지 번호와 인용 가능한 위치 확인 필요

## 7. 추출 원문 텍스트

```text
Evaluation of Sidewalk 
Autonomous Delivery 
Robot Interactions 
with Pedestrians and 
Bicyclists 
August 2022 A Research Report from the Pacific Southwest 
Region University Transportation Center 
 
Steven R. Gehrke, Northern Arizona University 
Brendan J. Russo, Northern Arizona University 
Christopher D. Phair, Northern Arizona University 
Edward J. Smaglik, Northern Arizona University 
 
 
 
 
 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
2 
 
TECHNICAL REPORT DOCUMENTATION PAGE 
1. Report No. 
PSR-21-16 
2. Government Accession No. 
N/A 
3. Recipient’s Catalog No. 
N/A 
4. Title and Subtitle 
Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians 
and Bicyclists 
5. Report Date 
August 10, 2022 
6. Performing Organization Code 
N/A 
7. Author(s) 
Steven R. Gehrke, Ph.D. (ORCID: 0000-0001-9355-5571) 
Brendan J. Russo, Ph.D. (ORCID: 0000-0002-0606-7973) 
Christopher D. Phair (ORCID: 0000-0001-8506-4799) 
Edward J. Smaglik, Ph.D. (ORCID: 0000-0002-7034-6619) 
 
8. Performing Organization Report No. 
TBD 
9. Performing Organization Name and Address 
METRANS Transportation Center 
University of Southern California 
University Park Campus, RGL 216 
Los Angeles, CA 90089-0626 
10. Work Unit No. 
N/A 
11. Contract or Grant No. 
USDOT Grant 69A3551747109 
12. Sponsoring Agency Name and Address 
U.S. Department of Transportation 
Office of the Assistant Secretary for Research and Technology 
1200 New Jersey Avenue, SE, Washington, DC 20590 
13. Type of Report and Period Covered 
Final report (08.15.2021 – 08.14.2022) 
14. Sponsoring Agency Code 
USDOT OST-R 
15. Supplementary Notes 
(none) 
16. Abstract 
Information and communication technology advancements and an increased demand for contactless deliveries after the Covid -
19 pandemic outbreak have resulted in the growing adoption of automated delivery services. Across university campuses, the 
deployment of sidewalk autonomous delivery robots (SADRs) has provided students, staff, and faculty a convenient last -mile 
delivery option. However, SADRs traverse campuses on paths designed for pedestrians and bicyclists, which could potentially 
result in conflicts among different pathway users and unsafe travel conditions. This report —comprising two studies—offers 
evidence on the objective safety and perceived comfort experienced by pedestrians and bicyclists interacting with SADRs on 
multi-use paths. In the first study, SADR interactions with human pathway users observed via field -recorded video collected at 
Northern Arizona University (NAU) campus were examined by employing the surrogate safety measure of post -encroachment 
time. The second study analyzed the reported comfort of SADR-involved interactions filmed from pedestrian and bicyclist 
perspectives and collected via the administration of a survey instrument to an NAU population with experience in the adoption 
of automated food delivery services and SADR-involved interactions. This report’s findings are intended to help inform new 
facility management strategies that support the safe introduction of SADRs on shared -use facilities in current and future settings. 
17. Key Words 
Sidewalk autonomous delivery robots; Pedestrians; Bicyclists; Post 
encroachment time; Level of comfort 
18. Distribution Statement 
No restrictions. 
19. Security Classif. (of this report) 
Unclassified 
20. Security Classif. (of this page) 
Unclassified 
21. No. of Pages 
54 
22. Price 
N/A 
Form DOT F 1700.7 (8-72) Reproduction of completed page authorized 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
3 
 
Contents 
Acknowledgements ......................................................................................................................... 5 
Abstract ........................................................................................................................................... 6 
Executive Summary ......................................................................................................................... 7 
Introduction .................................................................................................................................... 8 
Study 1: Observed Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and 
Bicyclists on Shared-Use Pathways ............................................................................................... 10 
Background ............................................................................................................................... 10 
Literature review....................................................................................................................... 11 
Methods .................................................................................................................................... 12 
Data and results ........................................................................................................................ 16 
Discussion ................................................................................................................................. 24 
Study conclusions ..................................................................................................................... 26 
Study 2: Reported Pedestrian and Bicyclist Comfort in Sharing Pathways with Sidewalk 
Autonomous Delivery Robots ....................................................................................................... 28 
Background ............................................................................................................................... 28 
Literature review....................................................................................................................... 29 
Methods .................................................................................................................................... 30 
Statistical analysis ..................................................................................................................... 32 
Data and results ........................................................................................................................ 33 
Study conclusions ..................................................................................................................... 43 
Conclusion ..................................................................................................................................... 46 
References .................................................................................................................................... 47 
Data Management Plan ................................................................................................................ 49 
Appendix: Perceptions of Autonomous Robots for Deliveries (PAR-D) survey instrument ......... 50 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
4 
 
About the Pacific Southwest Region University Transportation 
Center 
The Pacific Southwest Region University Transportation Center (UTC) is the Region 9 University 
Transportation Center funded under the US Department of Transportation’s University Transportation 
Centers Program. Established in 2016, the Pacific Southwest Region UTC (PSR) is led by the University of 
Southern California and includes seven partners: Long Beach State University; University of California, 
Davis; University of California, Irvine; University of California, Los Angeles; University of Hawaii; 
Northern Arizona University; Pima Community College. 
The Pacific Southwest Region UTC conducts an integrated, multidisciplinary program of research, 
education and technology transfer aimed at improving the mobility of people and goods throughout the 
region. Our program is organized around four themes: 1) technology to address transportation 
problems and improve mobility; 2) improving mobility for vulnerable populations; 3) Improving 
resilience and protecting the environment; and 4) managing mobility in high growth areas. 
U.S. Department of Transportation (USDOT) Disclaimer 
The contents of this report reflect the views of the authors, who are responsible for the facts and the 
accuracy of the information presented herein. This document is disseminated in the interest of 
information exchange. The report is funded, partially or entirely, by a grant from the U.S. Department of 
Transportation’s University Transportation Centers Program. However, the U.S. Government assumes no 
liability for the contents or use thereof. 
Disclosure 
Steven R. Gehrke (Principal Investigator), Brendan J. Russo (Co-Principal Investigator), and Edward J. 
Smaglik (Co-Principal Investigator) conducted this research titled, “Evaluation of Sidewalk Autonomous 
Delivery Robot Interactions with Pedestrians and Bicyclists” at Northern Arizona University. The research 
took place from August 15, 2021 to August 14, 2022 and was funded by a grant from the Pacific 
Southwest Region University Transportation Center in the amount of $97,421. The research was 
conducted as part of the Pacific Southwest Region University Transportation Center research program. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
5 
 
Acknowledgements 
The authors would like to express their gratitude for the generous financial contributions of the Pacific 
Southwest Region 9 University Transportation Center, which supported the research described in this 
report. The authors also would like to acknowledge the valuable contributions of several NAU student 
researchers. Michael P. Huff and Katherine R. Riffle assisted in collection of the field-recorded videos 
analyzed in the report’s first study (Chapter 2), while James M. Hollingsworth and Elijah A. Pinedo 
assisted in administering the survey instrument described in the report’s second study (Chapter 3). 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
6 
 
Abstract 
Information and communication technology advancements and an increased demand for contactless 
deliveries after the Covid-19 pandemic outbreak have resulted in the growing adoption of automated 
delivery services. Across university campuses, the deployment of sidewalk autonomous delivery robots 
(SADRs) has provided students, staff, and faculty a convenient last-mile delivery option. However, SADRs 
traverse campuses on paths designed for pedestrians and bicyclists, which could potentially result in 
conflicts among different pathway users and unsafe travel conditions. This report—comprising two 
studies—offers evidence on the objective safety and perceived comfort experienced by pedestrians and 
bicyclists interacting with SADRs on multi-use paths. In the first study, SADR interactions with human 
pathway users observed via field-recorded video collected at Northern Arizona University (NAU) campus 
were examined by employing the surrogate safety measure of post-encroachment time. The second 
study analyzed the reported comfort of SADR-involved interactions filmed from pedestrian and bicyclist 
perspectives and collected via the administration of a survey instrument to an NAU population with 
experience in the adoption of automated food delivery services and SADR-involved interactions. This 
report’s findings are intended to help inform new facility management strategies that support the safe 
introduction of SADRs on shared-use facilities in current and future settings. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
7 
 
Research Report 
Executive Summary 
Advancements in information and communication technologies coupled with public health concerns 
regarding Covid-19 transmission have sparked the introduction and growing demand for automated 
delivery services. On university campuses, sidewalk autonomous delivery robots (SADRs) have emerged 
in recent years as a last-mile freight service for students, faculty, and staff seeking the convenience of an 
on-demand delivery of meals or drinks to their residence or workplace. The commercial deployment of 
SADRs on American campuses, which occurred at Northern Arizona University (NAU) in March 2019, has 
grown in popularity due largely to the extensive pedestrian networks and population of technology-
savvy, young adults found in these settings. However, amongst this population segment and in these 
physical contexts, walking and bicycling is also common and may be hindered by the addition of SADRs 
on sidewalks and shared-use pathways primarily designed for pedestrians and bicyclists. 
This research project aims to provide early evidence on the observed and reported transportation safety 
concerns experienced by pedestrians and bicyclists who interact with SADRs along shared-use pathways 
on NAU’s campus. In this project’s first of two studies, five days of field-recorded videos were collected 
from 10 shared-use sites, with the frequency, location, and severity of SADR-involved interactions with 
human pathway users determined by adapting the surrogate safety measure of post-encroachment time 
(PET) to account for pathway user movements and trajectories at sites with ambiguity regarding travel 
lanes. The locations of observed SADR-involved interactions were then spatially depicted in relation to 
their physical context, while an ordered logit regression model of their PET-measured severity level was 
estimated as a function of conflict- and site-level characteristics. In the second study, self-reported data 
on SADR-related experiences and perceived comfort in sharing pathways with SADRs as an active 
traveler were collected by administering a tablet-based survey instrument to a convenient sample of the 
NAU population. The reported comfort levels were recorded as five-point Likert scale responses to four 
stated choice experiments that visualized a pedestrian or bicyclist making an evasive maneuver to avoid 
a collision with an oncoming SADR at two PETs, with these ordered outcome variables then modeled as 
a function of the socioeconomic attributes and SADR-related experiences of the survey’s respondents. 
Select findings from these two studies, which can help inform future research and practice related to the 
safe introduction or continued operation of SADRs in shared-use transportation settings, include: 
• Moderate and dangerous SADR-involved conflicts tend to cluster near sites with intersecting and 
narrow pathways without any delineation of what space human pathway users should occupy. 
• PET-measured severity of an SADR-involved interaction with a pedestrian or bicyclist tended to 
increase when an SADR was found to cross the human pathway user’s intended trajectory, with 
the pedestrian or bicyclist often taking an evasive action to avoid an SADR-involved collision. 
• While pedestrians were generally more comfortable than bicyclists in sharing paths with SADRs, 
individuals who reported discomfort in sharing paths with SADRs as a pedestrian or altered their 
paths in the past because of SADRs were less comfortable needing to evade an oncoming SADR. 
• Individuals who have frequently adopted autonomous food delivery services in the past tended 
to be more comfortable in taking evasive actions as a pedestrian or bicyclist to avoid potentially 
more-severe interactions with SADRs. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
8 
 
Introduction 
In March 2019, Northern Arizona University (NAU) became the second college campus in the United 
States to deploy a fleet of sidewalk autonomous robot delivery (SADR) vehicles capable of transporting 
meals and drinks from on-campus restaurants to its buildings and dormitories by an app-based request 
from faculty, staff, and students. Operated by Starship Technologies, these new low-speed automated 
delivery services utilize machine learning techniques, artificial intelligence recognition, and a suite of 
sensors to traverse shared-use paths on the NAU campus at a travel speed of four miles per hour (Figure 
1). The introduction of this new last-mile delivery freight technology brought immediate benefits related 
to consumer convenience and expanded meal access, while pressing concerns of climate change and the 
onset of the Covid-19 pandemic have placed a global spotlight on the importance of autonomous and 
electric delivery robots as a valuable step toward low-carbon and contactless freight delivery. However, 
SADR fleets presently operate on sidewalks and shared-use paths that were previously designated for 
exclusive use by pedestrians, bicyclists, and other human pathway users; therefore, creating potentially 
unsafe travel conditions among pathway users and ultimately traffic safety concerns for active travelers. 
Figure 1. Sidewalk autonomous delivery robots operated by Starship Technologies on NAU’s campus 
 
 
This research report, which is composed of two studies, investigates the objective and perceived safety 
concerns experienced by pedestrians and bicyclists traveling on shared-use paths with SADRs on NAU’s 
campus. In the first study, one week of field-recorded video from ten locations across the NAU campus 
were collected to offer baseline knowledge of the prevalence and severity of SADR-involved interactions 
with pedestrians and bicyclists. The severity of SADR-involved interactions was quantified by using the 
surrogate safety measure of post-encroachment time, which was then modeled as a function of conflict- 
and site-level characteristics to identify predictors of moderate or dangerous conflicts between SADRs 
and human pathway users. Findings from this first study, which provides initial real-world insights into 
the safety impacts of SADRs sharing pathways with pedestrians and bicyclists, are intended to help 
inform facility management strategies that are capable of supporting the safe introduction of these low-
speed automated freight devices on multimodal transportation facilities in current and future settings. 
In the second study, an original survey instrument was developed and administered to an NAU college 
population with real-world experience in the use of automated food delivery services and interaction 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
9 
 
with SADRs as active travelers. The survey instrument collected individual characteristics and responses 
to stated choice experiments of SADR-involved interactions from the perspectives of pedestrian and 
bicyclist. This study design permitted the identification of personal attributes and SADR-related 
experiences or perceptions that are associated with self-reported comfort in sharing pathways with 
SADRs from the perspective of an active traveler. Findings from this second study, which describes the 
profile of SADR service adopters and analyzes the reported level of comfort that individuals in a real-
world setting of SADR deployment have with this emergent freight technology as an active traveler, are 
intended to bolster a nascent evidence base on the use of emerging automated delivery robots and their 
possible safety impacts for pedestrians and bicyclists who share their transportation facilities. 
The design, implementation, and findings from each of these studies are described in the following two 
chapters of this report, which then concludes with a synthesis of this research report’s contributions. 
 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
10 
 
Study 1: Observed Sidewalk Autonomous Delivery Robot 
Interactions with Pedestrians and Bicyclists on Shared-Use 
Pathways 
Background 
In 2019, Starship Technologies first launched a commercial fleet of autonomous food delivery services 
on American college campuses (1). Northern Arizona University (NAU) was the second campus to 
welcome the operation of this new freight delivery technology—a fleet of 30 six-wheeled ground robots 
outfitted with cameras, sensors, and artificial intelligence capabilities that permit a mapping of its 
physical context and the application of its advanced object-detection system while traveling at four 
miles per hour (NAU 2019). The ability of these sidewalk autonomous delivery robots (SADRs) to deliver 
food orders to NAU students, faculty, and staff via a mobile device app has signified recent 
advancements in information and communication technologies. Public health concerns brought by the 
onset of the Covid-19 pandemic one year after the introduction of SADRs to NAU’s campus further 
amplified the demand for contactless delivery systems such as Starship’s low-speed automated delivery 
vehicles, whose service expanded from 500,000 deliveries worldwide at the start of the 2020 academic 
year to 3,000,000 deliveries by February 2022 (2). While increased SADR fleet sizes and service area 
expansions helped to meet this growing demand for more frequent online food deliveries, the 
heightened presence of these autonomous devices on pathways shared by pedestrians and bicyclists 
seeking safe routes for healthy, active travel also meant greater opportunity for unwelcomed conflict 
and further obstructions along ever-popular curbside spaces. 
Given the growing appeal of SADRs to consumers and marketplaces as well as the current paucity of city 
or state codes to regulate the safety feature requirements (e.g., braking systems, lights, size and weight 
limits) and operation (e.g., pedestrian yielding) of SADRs in public spaces (3), empirical evidence is 
needed to understand the local context and traffic conditions associated with SADR-involved 
interactions with human pathway users such as pedestrians and bicyclists. As sidewalk standards evolve 
and new curb management strategies arise, the management of transportation facilities designed 
primarily for pedestrians and bicyclists must account for the possibility that low-speed automated 
delivery services will vie for these public spaces along with emergent micromobility services for 
passenger travel. Accordingly, an immediate need exists for real-world research exploring the 
interactions between SADRs and human pathway users that can offer transportation officials and 
policymakers initial insights into the types of conflicts initiated by the introduction of new last-mile food 
and parcel delivery technologies and the physical settings most likely to create heightened conflict 
severities and unsafe active travel conditions. 
Recognizing the need for empirical research on real-world SADR operations, the objectives of this study 
are twofold. First, this study aims to generate new evidence regarding the traffic safety experienced by 
active travelers who share pathways with these recently deployed autonomous food delivery services. 
This study objective was attained by collecting field-recordings of SADRs operating in mixed traffic 
settings and adapting a surrogate safety measure (SSM) to define severity of any observed SADR-
involved incident. The spatial description of these incidents across ten sites on NAU’s main campus in 
Flagstaff, Arizona and the statistical modeling of SADR-human pathway user conflict severity as a 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
11 
 
function of conflict- and site-level characteristics will help to address the second study objective, which 
is to inform future mitigation and facility management strategies that can guide the safe operation of 
SADRs in new multimodal settings. 
Literature review 
Empirical studies of traffic safety are often limited by the relative rare nature and randomness of 
crashes as well as potential inconsistencies related to incident reporting. These conditions and a desire 
to identify serious interactions that may not result in a crash have supported the use of SSMs as an 
alternative for identifying and analyzing traffic safety issues, especially those which concern pedestrians 
and bicyclists (4). Post encroachment time (PET) is one SSM that permits an evaluation of the severity of 
a traffic incident based on the immediacy in which a crash was avoided (5). Previous research has 
defined PET as the time elapsed from the moment when a vehicle departs a potential collision site to 
the moment of arrival at the potential collision site by the conflicting vehicle (6). While early PET 
applications centered on the study of vehicle traffic safety, recent research has evaluated the usefulness 
of this SSM in shared-use, multimodal settings primarily occupied by pedestrians and bicyclists, including 
a four-day analysis of pedestrian-bicyclist interactions in a shared space on the campus of McGill 
University (7) and a 12-hour analysis of interactions amongst pedestrians and bicyclists (traditional and 
electric) in Shenzhen, China (8). These past studies and others support the validity of using PET as a SSM 
for analyzing the physical conditions and user characteristics associated with active traveler safety in 
shared-use settings. However, to the best knowledge of this study’s authors, no research to-date has 
explored the safety impacts faced by pedestrians or bicyclists in relation to the recent introduction of 
SADRs in shared-use environments. 
Past studies have adopted PET as a SSM to assess vehicle-pedestrian conflicts. Chen et al. (9) assessed 
pedestrian safety conditions associated with right-turning vehicles at two intersections in Beijing, China 
by collecting two hours of unmanned aerial vehicle video footage. The results from their analysis of 
2,473 pedestrians and 2,897 right-turning vehicles demonstrated that PET was able to accurately assess 
pedestrian-vehicle conflicts at crosswalks and that danger increased for pedestrians when the right-
turning angle of the vehicle increased (9). In a second study, Ni et al. (10) evaluated video collected at 
three intersections in Shanghai, China that encompassed a total of 1,144 vehicle-pedestrian interactions. 
For this second study, the authors considered interactions with a PET value of less than three seconds as 
a conflict or critical event, with these more severe vehicle-pedestrian interactions conveying clear site-
level spatial patterns (10). A third study, which evaluated video data of over 28,000 vehicle-pedestrian 
interactions at four unsignalized intersections in Poland across two months, highlighted PET as a 
promising indicator of pedestrian safety in settings without traffic controls (11). 
Other studies have adopted PET as an appropriate SSM for better understanding the patterns and 
predictors of interactions between motor vehicles and bicyclists. Stipancic et al. (12) evaluated 1,514 
bicyclist-vehicle interactions extracted from passive video collected at seven intersections in Montreal, 
Canada, with PET adopted as a SSM suited for vulnerable road user safety. Results from this study, 
which categorized interactions as normal, conflicts, or dangerous conflicts based on calculated PET 
value, found that bicycle and vehicle speed along with select human pathway user factors predicted an 
increase in conflict severity (12). Another Montreal-based study (13), which collected 90 hours of video 
from 23 intersections to evaluate the effectiveness of bike lanes in protecting bicyclists from turning 
vehicles, also categorized interactions based on PET values: very dangerous (PET ≤ 1.5s), dangerous (1.5s 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
12 
 
< PET ≤ 3s), mild (3s < PET ≤ 5s) and no interaction (PET > 5s). The study’s estimation of ordered logistic 
regression models specifying site-level characteristics including bicyclist exposure and bike lane 
conditions found higher PET values (i.e., safer traffic conditions) when bike lanes were located on the 
left side of vehicular traffic rather than the opposing side (13). The adoption of PET and other SSMs has 
been operationalized in other traffic safety research including a study of 23 hours of video at a Kunming, 
China intersection that examined vehicle-involved interactions with powered two-wheelers (14). The 
authors suggest that present applications of PET or other time-proximity safety indicators that utilize 
fixed geographies for conflict measurement may be limited if mixed road users share smaller spaces and 
take evasive actions to avoid a collision (14). 
As noted above, recent studies have evaluated pedestrian and bicyclist safety in smaller shared-use 
settings. Bietel et al (7) extracted 2,739 pedestrian-bicyclist interactions from passively collected video 
and applied several SSMs including a semi-automated adaptation of the traditional vehicle-involved PET 
metric based on walking trajectories. However, the authors noted their effort to adapt PET 
measurements to smaller shared spaces may be insufficient alone for determining conflict severity in 
shared spaces. Nikiforiadis et al. (15) similarly introduced a new methodology for assessing pedestrian-
bicyclist conflicts in shared spaces known as the hindrance concept, which involves defining an 
approximate one-meter radius around the two active travelers involved in an interaction. Meanwhile, 
the aforementioned study by Liang et al. (8) of active travelers in Shenzhen, used the Dutch Objective 
Conflict Technique for Operation and Research method to define and evaluate vulnerable road user 
conflicts. From this review, it is evident that the assessment of pedestrian and bicyclist safety conditions 
in shared-use settings has been explored but that (i) limitations persist regarding the translation of PET 
from an SSM used in vehicle-based studies to a traffic safety indicator in multimodal settings where 
users are not necessarily confined to a fixed travel lane and (ii) previous vulnerable road user safety 
research has yet to explore the implications of low-speed automated delivery vehicles entering public 
spaces that have thus far largely been occupied by human travelers. 
Methods 
PET measurement 
The SSM of PET was adopted in this study to identify and quantify interactions between SADRs and 
human pathway users (e.g., pedestrians, bicyclists). To measure the PET associated with an observed 
interaction, research team members (researchers) analyzed field-collected video with timestamps from 
a set of data collection points. The first step toward identifying interactions and measuring their 
associated PET was to generate a ‘bounding box’ for each video collection site. The spatial definitions of 
site-specific bounding boxes were determined by researchers, using physical landmarks visible to a video 
reviewer who would need to determine if the trajectories of an SADR and human pathway user crossed 
within the defined boundary. The bounding boxes in this study averaged 1,943 square feet (ranging from 
503 to 3,550 square feet), with size variations attributed to the angle and height of stationary 
videorecorders at each site and site-level decisions regarding the inclusion or exclusion of intersecting 
pathways. 
Interactions between SADRs and human pathway users were later observed in bounding boxes, with an 
associated PET measurement identified for SADR-involved interactions. The PET measure was 
determined through a multi-step process in which researchers first identified a ‘conflict zone’ within 
each video collection site’s predetermined bounding box. For this study, a conflict zone was determined 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
13 
 
to be an area where the observed trajectories of an SADR and human pathway user crossed one another 
within approximately five seconds. Once an incident-specific conflict zone was identified, the 
timestamps of when the first pathway user departed the conflict zone (time1) and when the second 
user arrived to the conflict zone (time2) were recorded, with the PET of the given interaction then 
calculated as the difference between the two recorded timestamps (PET = time2 - time1). Figure 2 
visualizes this sequence for two SADR-involved conflict types with a pedestrian. When viewing recorded 
interactions, researchers were able to pause, rewind, and fast forward videos, allowing for greater 
precision in interaction identification and PET measurement. 
Figure 2. Illustration of PET measurement for two types of SADR-pedestrian conflicts 
 
 
Video collection and review 
The observation of SADR interactions with human pathway users and associated measurement of PET 
was conducted after the collection and review of video recorded on NAU’s main campus. Video 
collection was undertaken after identifying study area sites where a reasonable number of interactions 
between SADRs and other pathway users could be anticipated. After consultation with NAU facility 
management staff, researchers selected ten sites along highly trafficked pathways in locations near 
significant SADR origins and destinations (e.g., student unions, residential halls). As shown in Figure 3, 
six of the study sites were located on the northern part of NAU’s campus, while four were located on the 
southern portion of campus in proximity to a popular shared-use path leading to the south campus 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
14 
 
student union. At each of the ten data collection sites, passive video was recorded using high-definition 
video cameras affixed to extended telescoping poles that were fastened to stationary signs or utility 
poles adjacent to the bounding boxes. The video cameras were positioned approximately ten feet above 
the ground, which permitted relatively inconspicuous observations of the natural interactions between 
SADRs and human pathway users. Videos were recorded from 9am to 6pm at each site over five days in 
late September/early October 2021, under clear weather conditions and while in-person classes were 
held. This daily observation period coincides with Starship SADR delivery times, which may begin earlier 
or continue later depending on meal preparation location. To help streamline data reduction efforts, 
video review was only conducted during three time periods that coincided with approximate mealtimes 
when SADRs would be in transit and class transition times where students would also be traveling on the 
shared pathways: 9:00-10:30am, 11:00am-2:00pm, and 4:30-6:00pm. After applying this data reduction 
step and accounting for periods where continuous video collection was interrupted (i.e., loss in external 
battery charge), a total of 187 hours of video across the sites was available for review and analysis. 
Figure 3. Video collection sites on the north and south campus of NAU 
 
 
After the final study observation period was determined, all field-collected recordings, which were 
parsed into 15-minute video clips, were reviewed by researchers in multiple phases. In the first phase of 
video review, each site was assigned to a researcher who manually recorded the volumes of SADRs, 
pedestrians, bicyclists, and other pathway users in each 15-minute video that traveled across one 
predetermined edge of each study site’s bounding box. For the second video review phase, any 15-
minute video with one or more observed SADR in the volume count was reviewed by two researchers to 
identify SADR interactions with human pathway users and record the timestamps associated with each 
pathway user exiting or entering the conflict zone. Here, researchers applied the PET methodology 
described in the previous section for all SADR-involved interactions that were judged to have produced a 
PET value of five seconds or less. Following the second video review phase, the researchers who 
reviewed videos collected for a given site jointly conducted the following steps to help ensure internal 
consistency in interaction identification and associated characteristics: 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
15 
 
• If an interaction with a recorded PET value difference less than one second was identified by 
two researchers, then the lower PET value was retained. 
• If an interaction with a recorded PET value difference greater than one second was identified by 
two researchers, then the interaction was reviewed by both researchers until agreement on the 
PET value was reached. 
• If an interaction with a recorded PET value of five seconds or less was originally identified by 
only one researcher, then the interaction was reviewed by both researchers until agreement on 
the PET value was reached (removing interactions with a PET value greater than five seconds 
from the final sample). 
During the final video review phase, the two researchers assigned to review the videos for a particular 
site also recorded conflict-level characteristics regarding the first and second pathway user type to enter 
a conflict zone, travel direction of the second pathway user in relation to the first pathway user, evasive 
actions taken by both pathway users, and whether the SADR-involved interaction was intentionally 
initiated by a human pathway user. Intentional interactions were removed from the final study sample. 
In the final sample, PET values for retained SADR-involved interactions were categorized into discrete 
severity levels. Based on prior research (13;16), observed interactions with a PET value of 1.5 seconds or 
less were categorized as a dangerous conflict, while SADR interactions with human pathway users that 
produced a PET value above 1.5 seconds and less than or equal to three seconds were categorized as 
moderate conflicts. All recorded interactions with a PET value greater than three seconds were deemed 
to be normal interactions. 
Spatial description of SADR interactions and observation sites 
After a recognition and PET classification of SADR interactions with human pathway users was 
completed, a visual depiction of interaction sites and measurement of site-level characteristics was 
undertaken. The spatial depiction of observed SADR interactions with pedestrians and bicyclists and the 
dimensions of the bounding box for each site were generated within a geographic information systems 
(GIS) environment. A visual inspection of the location of each SADR-involved interaction in the final 
study sample, which were determined by a review of the field-recorded videos and subsequent manual 
placement in a GIS software, allowed researchers to both identify visual patterns or clusters of 
interactions across different severity levels and examine whether the location of recorded interactions 
appeared to be associated with any urban design or transportation network characteristics of a video 
collection site. To complement any descriptive findings resulting from the spatial inspection of SADR 
interactions, characteristics related to bounding box definitions were also recorded as potential 
predictors in a statistical model of PET severity. These site-level characteristics include the presence of a 
designated bike lane, the width of the sidewalk (or shared-use path), the presence of a lateral barrier 
(e.g., planter box) to the pathway, and the number of pathway intersections located along the perimeter 
of a site’s designated bounding box. 
Statistical analysis 
A statistical analysis of different conflict- and site-level characteristics that predicted PET severity was 
then performed to offer further insights into the physical context and conditions associated with SADR 
conflicts with human pathway users. Given the limited number of unintentional SADR interactions 
observed in this study (n=201), an analytic decision was made to pool the final sample to include 
interactions among SADRs and all human pathway users. Moreover, to offer study findings that may be 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
16 
 
more immediately translated to practitioners seeking insights into what SADR interactions are more 
worrisome to pedestrians and bicyclists than others and what factors may predict an actual conflict, the 
outcome variable of interest for this statistical analysis is the ordered severity level of each observed 
SADR-involved interaction (0 = no interaction, 1 = moderate conflict, and 2 = dangerous conflict). While 
the choice of thresholds to delineate the three severity levels are somewhat subjective and arbitrary, 
their selection can be justified by previous research (13) and analytic need for an ordered logistic 
regression model to meet the assumption of proportional odds. The ordered logistic model specified in 
this statistical analysis is expressed in Equation 1 (17) and is an extension of a logistic regression model 
applied when the dependent variable is an ordered-response with more than two discrete levels: 
𝑃(𝑦𝑖 > 𝑗) =
𝑒𝑥𝑝(𝑋𝑖𝛽′−𝜙𝑗)
1+𝑒𝑥𝑝(𝑋𝑖𝛽′−𝜙𝑗) 𝑗 = 1, 2, … , 𝑀 − 1 (1) 
 
where 𝑗 is the interaction severity level, 𝑋𝑖 is a vector of observed conflict- and site-level characteristics, 
𝛽 is a vector of estimated parameters, 𝜙𝑗 are breakpoints associated with the severity level thresholds, 
and 𝑀 is the number of categories of the ordered-response variables. 
The final ordered logistic model for the pooled study sample was specified via a two-step process. First, 
the Spearman correlation value for each conflict- and site-level characteristic with the severity level 
outcome was calculated and all marginally significant characteristics (p<0.10) were added to a full model 
specification. Second, a backwards elimination process was conducted to iteratively remove the 
predictor with the highest p-value from the previously specified model until all remaining independent 
variables were significant predictors of the ordered outcome variable. 
Data and results 
Description of SADR interactions with pedestrians and bicyclists 
A distribution of the PETs measured in this study’s sample of 192 SADR interactions with pedestrians 
and bicyclists is shown in Figure 4. Of note, nine interactions in the final sample (n=201) involved an 
SADR and human pathway user who was not walking or bicycling (e.g., e-scooter rider). For interactions 
involving a pedestrian (n=169) or bicyclist (n=23), 106 observations were categorized as either a 
moderate (level 1) or dangerous (level 2) conflict. Pedestrians were involved in 38 (or 95%) of the 40 
dangerous conflicts, with 12 of these level 2 interactions resulting in a PET of zero seconds. There were 
no observed SADR-bicyclist interactions with a PET of zero seconds, which represents either a crash 
between the two pathway users or an incident in which a human pathway user’s body was directly 
above the SADR at the identified point of conflict. The two observed dangerous conflicts involving an 
SADR and bicyclist had a PET measurement between 1.0 and 1.5 seconds. Most observed SADR-bicyclist 
interactions were categorized as moderate conflicts (52%), while 32% SADR-pedestrian interactions 
were similarly categorized as level 1 interactions. For interactions visualized in Figure 4 46% and 39% of 
SADR interactions with pedestrians and bicyclists, respectively, were categorized as a normal interaction 
(level 0). 
Figure 4. Distribution of observed SADR-involved interactions with human pathway users by conflict 
severity level 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
17 
 
 
 
Table 1 provides a summary of the conflict- and site-level characteristics observed in the study sample of 
201 SADR interactions with human pathway users (e.g., pedestrians, bicyclists) across the 10 sampling 
locations. On average, the PET for a recorded interaction was 2.79 seconds. Regarding the time of day, 
most of the sampled interactions between SADRs and human pathway users occurred during the 
afternoon (68%), which was also the longest of the three daily observation periods. In most interactions 
(57%), the SADR was the first pathway user to reach the conflict area and thus deemed to have initiated 
the conflict with the human pathway user. Of the three types of interactions captured in our study, 
nearly one half (47%) were crossing conflicts, with the remaining interactions either signifying a head-on 
meeting in which the two pathway users were traveling in opposite directions (37%), or one pathway 
user was attempting to overtake another pathway user traveling in the same direction (15%). 
 
Table 1. Descriptive statistics of observed SADR interactions and video collection sites 
Variable Mean SD Min Max 
Post encroachment time (seconds) 2.79 1.30 0 5 
Conflict Characteristics 
 Robot first to conflict 0.57 0.50 0 1 
 Conflict direction: Same 0.15 0.36 0 1 
 Conflict direction: Opposite 0.37 0.48 0 1 
 Conflict direction: Crossing 0.47 0.50 0 1 
 Time of day: Morning (9:00am-10:30am) 0.12 0.33 0 1 
 Time of day: Afternoon (11:00am-2:00pm) 0.68 0.47 0 1 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
18 
 
 Time of day: Evening (4:30pm-6:00pm) 0.20 0.40 0 1 
 Evasive maneuver (Pathway user 1): No action 0.44 0.50 0 1 
 Evasive maneuver (Pathway user 1): Complete stop 0.10 0.30 0 1 
 Evasive maneuver (Pathway user 1): Deceleration 0.10 0.29 0 1 
 Evasive maneuver (Pathway user 1): Acceleration 0.01 0.10 0 1 
 Evasive maneuver (Pathway user 1): Swerve 0.35 0.48 0 1 
 Evasive maneuver (Pathway user 2): No action 0.33 0.47 0 1 
 Evasive maneuver (Pathway user 2): Complete stop 0.26 0.44 0 1 
 Evasive maneuver (Pathway user 2): Deceleration 0.10 0.30 0 1 
 Evasive maneuver (Pathway user 2): Acceleration 0.01 0.07 0 1 
 Evasive maneuver (Pathway user 2): Swerve 0.30 0.46 0 1 
Site Characteristics 
 Exposure (15-min window): Robots 6.93 3.72 0 18 
 Exposure (15-min window): Pedestrians 100.90 88.85 5 634 
 Exposure (15-min window): Bicyclists 19.51 14.56 0 57 
 Exposure (15-min window) share: Robots 0.08 0.07 0.00 0.34 
 Exposure (15-min window) share: Pedestrians 0.70 0.14 0.06 0.95 
 Exposure (15-min window) share: Bicyclists 0.15 0.08 0.00 0.40 
 Presence of bike lane 0.86 0.35 0 1 
 Sidewalk width: Less than 10 feet 0.17 0.38 0 1 
 Sidewalk width: 10-20 feet 0.48 0.50 0 1 
 Sidewalk width: 20 feet or more 0.34 0.48 0 1 
 Presence of lateral pathway barrier 0.01 0.12 0 1 
 Pathway intersections: 0 0.22 0.41 0 1 
 Pathway intersections: 1 0.17 0.38 0 1 
 Pathway intersections: 2 0.28 0.45 0 1 
 Pathway intersections: 3 0.33 0.47 0 1 
 
 
Irrespective of conflict type, 44% of those pathway users who initiated an interaction were found to 
have taken no action, while only one third (33%) of pathway users who were second to the conflict point 
were observed to have not taken any evasive actions. The most common evasive action observed in 
SADR conflicts with human pathway users was an abrupt change in direction (swerve), with 35% and 
30% of first and second pathway users, respectively, observed to have taken this action in an 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
19 
 
interaction. Ten percent of first and second pathway users in an observed interaction chose to 
decelerate in avoidance of a crash, while it was more common for the second pathway user to a conflict 
point to make a complete stop (26%) than the pathway user who initiated the interaction (10%). In turn, 
only 1% of the first or second pathway users to reach the conflict point accelerated their travel speed to 
avoid any crash, with no pathway users in an interaction found to have reversed their travel direction 
(not shown in Table 1). Generalizing the above descriptive findings, the average interaction was 
classified as a moderate conflict (level 1) in which the SADR initiated the conflict with a human pathway 
user by crossing in front of the path taken by the pedestrian or bicyclist, with both pathway users likely 
to have taken some evasive action to avoid a crash. 
In complement to the above evasive maneuver summary of the study sample used for statistical 
modeling, Table 2 offers a cross-tabulation of the evasive maneuvers taken in SADR interactions of 
varying levels of PET classification for the first and second pathway users to reach a conflict point. 
Regarding interactions between SADRs and pedestrians in which the SADR was first to the conflict point 
(pathway user 1), the pedestrian was less likely to make an evasive action as the PET measurement 
neared zero seconds. This outcome may be the result of the pedestrian not recognizing the approaching 
SADR, recognizing that the SADR has made the evasive maneuver, or anticipating that the path of the 
SADR will not result in a crash. Regardless of the interaction type, the most popular evasive action taken 
by the pedestrian when the SADR reached the conflict point first was to swerve. For SADR-pedestrian 
interactions that were initiated by a pedestrian (44% of all SADR-pedestrian interactions), the pedestrian 
swerved from the SADR in every dangerous conflict, 78% of moderate conflicts, and 65% of interactions 
where no conflict was determined. 
Akin to SADR interactions with pedestrians, most bicyclist interactions with SADRs (61%) were initiated 
by the SADR, with the two dangerous conflicts observed in this sample characterized by a robot reaching 
the conflict point first. No action was taken by the bicyclist in either of the two dangerous conflicts, with 
two-thirds of bicyclists in observed moderate conflicts (n=6) and normal interactions (n=2) similarly 
conducting no evasive maneuver. If an evasive action was taken by a bicyclist in a level 1 or level 0 
incident initiated by an SADR, that bicyclist was observed to have abruptly changed direction. In all three 
observed moderate conflicts where a bicyclist reached the identified conflict point before the SADR, the 
bicyclist was found to have swerved. For the remaining six SADR-bicyclist interactions (no conflict) 
where a bicyclist initiated an interaction, there were four (67%) occasions in which the human pathway 
user took no evasive action 
By examining site characteristics associated with the study sample of observed interactions (Table 1), 
insights into the context surrounding SADR interactions with human pathway users can be offered. 
Across all study observations, the average interaction occurred in a 15-minute window of video review 
in which 100 pedestrians, 20 bicyclists, and seven SADRs were enumerated at the particular site. On 
average, the site with the observed conflict had a dedicated bike lane (86%) and a sidewalk width that 
was at least 10 feet (82%), with little presence of a lateral pathway barrier (1%). One third (33%) of 
interactions noted in this study occurred inside a bounding box with three or more intersections, while 
45% of all recorded interactions were observed at a site with one or two pathway intersections. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
20 
 
Table 2. Evasive maneuvers of pedestrians and bicyclists in SADR interactions 
Pedestrian Interactions: Pathway User 1: SADR Pathway User 2: SADR 
Pedestrian’s Evasive Maneuver Dangerous 
Conflict 
Moderate 
Conflict 
No 
Conflict 
Dangerous 
Conflict 
Moderate 
Conflict 
No 
Conflict 
 No action 14 14 10 0 3 15 
 Complete stop 2 0 1 0 0 0 
 Deceleration 1 2 0 0 0 0 
 Acceleration 0 0 0 0 1 1 
 Swerve 11 20 20 10 14 30 
 Back-up 0 0 0 0 0 0 
 Total Interactions 28 36 31 10 18 46 
Bicyclist Interactions: Pathway User 1: SADR Pathway User 2: SADR 
Bicyclist’s Evasive Maneuver Dangerous 
Conflict 
Moderate 
Conflict 
No 
Conflict 
Dangerous 
Conflict 
Moderate 
Conflict 
No 
Conflict 
 No action 2 6 2 0 0 4 
 Complete stop 0 0 0 0 0 0 
 Deceleration 0 0 0 0 0 0 
 Acceleration 0 0 0 0 0 0 
 Swerve 0 3 1 0 3 2 
 Back-up 0 0 0 0 0 0 
 Total Interactions 2 9 3 0 3 6 
 
 
Description of sites with SADR interactions 
A site-by-site overview of the 10 sampling locations is provided in Table 3, with details on characteristics 
of the pathways as well as 15-minute counts of observed pathway users and interactions between 
SADRs and pedestrians, bicyclists, and other human travelers. Regarding the site characteristic of 
sidewalk width, three of the north campus video collection areas (sites 1, 2, and 5) and three of the 
south campus areas (sites 7, 8, and 10) had sidewalks wider than 15 feet. However, both site 4 (north 
campus) and site 9 (south campus) had sidewalk widths less than 10 feet. Each of the two latter sites 
had designated bike lanes and no pathway intersections within their selected bounding boxes. 
Separated bike lanes in which SADRs are not programmed to travel within existed at eight of the video 
collection sites, with the exceptions of sites 6 (north campus) and 9 (south campus). Akin to site 9, the 
bounding box at site 6 also did not capture any pathway intersections, which was also characteristic of 
sites 2, 4, and 7. Site 7 was the only video collection area to have a vertical barrier adjacent to its 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
21 
 
pathway. Only three of the sites had more than one pathway intersection, with sites 3 and 10 having 
two intersecting paths and site 5 having three intersections. 
 
Table 3. Video collection site characteristics and observed exposure and SADR interaction information 
Site Site Characteristics 15-min Exposure Counts Observed SADR Interactions 
1 
Sidewalk width (ft) 19.0 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 2.2 243.7 26.3 17.5 Ped 8 1 2 
Lateral path barrier No Min 0 30 5 1 Bike 0 0 0 
Path intersections 1 Max 7 938 90 51 Other 0 0 0 
2 
Sidewalk width (ft) 19.4 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 2.2 209.4 26.9 17.5 Ped 0 0 0 
Lateral path barrier Yes Min 0 17 3 2 Bike 0 0 0 
Path intersections 0 Max 9 746 109 51 Other 0 0 0 
3 
Sidewalk width (ft) 11.2 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 2.3 107.1 18.3 4.6 Ped 10 8 5 
Lateral path barrier No Min 0 11 2 0 Bike 2 2 1 
Path intersections 2 Max 9 228 59 16 Other 1 0 0 
4 
Sidewalk width (ft) 6.9 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 3.0 123.4 6.4 4.5 Ped 3 1 0 
Lateral path barrier No Min 0 24 1 1 Bike 0 1 0 
Path intersections 0 Max 9 373 25 13 Other 0 0 0 
5 
Sidewalk width (ft) 20.7 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 5.4 61.0 18.6 8.8 Ped 25 16 14 
Lateral path barrier No Min 0 5 4 0 Bike 2 3 1 
Path intersections 3 Max 17 174 70 53 Other 2 1 2 
6 
Sidewalk width (ft) 12.5 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane No Mean 3.4 84.4 3.3 1.7 Ped 6 4 8 
Lateral path barrier No Min 0 27 0 0 Bike 2 2 0 
Path intersections 0 Max 12 253 10 6 Other 1 0 0 
7 
Sidewalk width (ft) 20.9 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 4.9 39.5 12.3 4.8 Ped 1 1 1 
Lateral path barrier Yes Min 1 15 2 0 Bike 0 0 0 
Path intersections 0 Max 11 86 34 15 Other 0 0 0 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
22 
 
8 
Sidewalk width (ft) 15.4 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 7.0 55.3 15.9 7.6 Ped 13 6 5 
Lateral path barrier No Min 0 13 0 1 Bike 1 2 0 
Path intersections 1 Max 18 171 53 33 Other 1 0 0 
9 
Sidewalk width (ft) 9.8 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane Yes Mean 6.1 76.0 18.9 9.3 Ped 9 17 3 
Lateral path barrier No Min 0 25 4 1 Bike 0 1 0 
Path intersections 0 Max 17 219 58 30 Other 0 0 0 
10 
Sidewalk width (ft) 18.0 SADR Ped Bike Other PET=0 PET=1 PET=2 
Separated bike lane No Mean 3.9 18.4 11.4 6.0 Ped 2 0 0 
Lateral path barrier No Min 0 2 2 0 Bike 2 1 0 
Path intersections 2 Max 14 59 36 24 Other 1 0 0 
 
 
In general, the highest volume of pedestrians was observed on the six north campus sites, ranging from 
244 pedestrians per hour at site 5 to 975 pedestrians per hour at site 1. All sites on the southern portion 
of campus had lower pedestrian exposure counts than north campus sites, with the exception of site 9 
(304 pedestrians per hour). In terms of bicyclist exposure counts, the distribution was more balanced 
across all campus sites. Sites 4 and 6 on north campus had fewer bicyclists, with one-hour exposure 
counts of 13 and 26 bicyclists, respectively. North campus sites 1 and 2, however, had the highest counts 
of bicyclists, with one-hour averages at each site slightly above 100 bicyclists, whereas the highest count 
of bicyclists on south campus was observed at site 9 (76 bicyclists per hour). Turning to SADR counts, 
sites 1 and 2, which are located north of most campus dining options and on-campus residences, had 
the fewest recorded SADRs, while sites 8 and 9, which are located on a multiuse path connecting the 
south campus student union and several on-campus dormitories, had the highest count of SADRs. 
Through a site-level investigation of SADR interactions with pedestrians, site 5 was found to have the 
most total interactions (n=55), with 16 interactions categorized as moderate conflicts and 14 
interactions categorized as dangerous conflicts. Site 5 was also the location of one of the two observed 
SADR-bicyclist level 2 interactions. Figure 5 shows these locations of SADR-pedestrian and SADR-bicyclist 
interactions, by their PET category. At site 5, all pedestrian and bicyclist interactions with SADRs 
occurred on sidewalks, which are the facilities that these food delivery services are programmed to 
traverse. However, clusters of interactions are found at the three intersecting paths, with a set of 
dangerous conflicts observed at the two corners of a service road located to the west that intersects the 
shared-use facility. Other SADR-pedestrian interactions categorized as dangerous conflicts are located 
south of the intersecting service road near the entrance to the academic building and along the eastern 
sidewalk between the two other intersecting paths. In terms of SADR-bicyclist interactions, multiple 
interactions are found in the northwestern corner of the bounding box, which is also the site of a bike 
parking corral. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
23 
 
Figure 5. Spatial depiction of SADR-involved interactions with pedestrians and bicyclists at site 5 
 
 
Further examination of Table 3 reveals that site 9, which had the second highest volume of SADRs and 
second narrowest sidewalk width of the ten sites, had the second highest count of SADR-pedestrian 
interactions (n=29). Sites 3 and 8 also had more than 20 SADR-pedestrian interactions, with each site 
having at least one pathway intersection and being on the lower-half of sites in terms of sidewalk width. 
Site 6, in turn, was found to have the second most level 2 SADR-pedestrian conflicts, with this site having 
no pathway intersections but also no separated bike lane and a sidewalk width of 12.5 feet. Site 6 also 
had the third highest number of SADR-bicyclist conflicts, which may be due to a mixing of all pathway 
users on a narrow shared-use facility. Meanwhile, site 3, which has a separated bike lane but two 
intersections, had the second highest number of SADR-bicyclist interactions (n=5). Of note, the two sites 
with the greatest bicyclist volumes (sites 1 and 2) had no recorded SADR-bicyclist interactions despite 
having comparable SADR volumes to site 3. Descriptively, this study finding and an absence of SADR-
bicyclist interactions at site 7 may be related to the presence of separated bike lanes, wider sidewalks, 
and limited pathway intersections that when taken together signify ample space for overtaking actions 
and minimal opportunities for SADR and human pathway user routes to cross. 
Modeled interactions of SADR interaction severity 
Model results of SADR interactions with pedestrians, bicyclists, and other human pathway users are 
detailed in Table 4. The final model revealed a statistically significant improvement over the constants 
only model (χ2=28.72, p<0.001), with three predictors related to conflict characteristics that generally 
agreed with the descriptive statistics of the aggregate data set. Extrapolating model results of observed 
SADR interactions, the severity of SADR-involved interactions tended to increase if the robot was the 
first pathway user to reach the conflict point. As previously mentioned, a majority of interactions 
observed in the study sample were initiated by the SADR pathway user. Model results also found that 
the evasive action of the first pathway user to reach an identified conflict point was more likely to be a 
swerve in travel direction as the PET associated with an interaction neared zero seconds. Descriptively, if 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
24 
 
an SADR-pedestrian interaction initiated by an SADR was defined as a moderate or dangerous conflict, 
then the robot was more likely to swerve rather than take no evasive action. Finally, model results 
suggested that the severity of an SADR interaction decreased if the robot and the human pathway user 
were traveling in opposite directions. This model finding is likely attributable to the increased likelihood 
that a human pathway user can see the approaching SADR from a safe distance and make a normal 
adjustment to their travel trajectory. 
 
Table 4. Ordered logit model results 
Variable Beta SE CI: 2.5% CI: 97.5% 
Conflict Characteristics 
 Robot first to conflict 1.75 0.39 1.01 2.55 
 Conflict direction: Opposite -0.71 0.34 -1.38 -0.05 
 Evasive maneuver (Pathway user 1): Swerve 1.00 0.44 0.15 1.89 
Intercepts 
 Threshold: 0 and 1 0.91 0.37 
 Threshold: 1 and 2 2.59 0.40 
Model Summary 
 Number of observations 201 
 Log likelihood -196.91 
 Log likelihood (constants only) -211.26 
 Akaike information criterion 403.81 
 
 
Discussion 
This study helps identify the traffic safety concerns of pedestrians and bicyclists sharing pathways with 
SADRs and offers evidence into the challenges of operating these new delivery technologies in a real-
world setting. Specifically, by recognizing the most common types and patterns of SADR interactions 
with pedestrians and bicyclists, improvements in SADR route selection and facility management 
practices that strive to reduce the number and severity of SADR conflicts with human pathway users can 
be pursued by autonomous food delivery service providers and transportation planners and engineers. 
In examining the spatial distribution of observed interactions and the statistical modeling results, SADR-
pedestrian conflicts tended to cluster at intersections of sidewalks and other non-motorized pathways 
and occurred when an SADR was crossing in front of or overtaking a pedestrian on a sidewalk. To help 
reduce the prevalence of crossing conflicts, SADR routes that prioritize the parallel travel of these 
devices along pedestrian corridors and minimize the number of high-activity sidewalk crossings should 
be programmed when possible. SADR routes must also factor in the width of sidewalks to ensure that 
adequate space is available for an SADR to safely overtake a slower moving pedestrian without changing 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
25 
 
the human pathway user’s trajectory. In instances where a common trip destination cannot be served 
without an SADR traveling on a sidewalk designed for one pedestrian in each travel direction, actions to 
widen the sidewalk should be considered. 
 While an extensive pedestrian network spans most of NAU’s main campus, there is less 
dedicated infrastructure for bicyclists who often share facilities principally designed for walking. In this 
study, SADR interactions with bicyclists were common where bike network gaps exist and bike parking 
facilities are located. In response, modifications to or considerations in SADR routes that favor placing 
the autonomous technology on the shared-use path side that produces fewer bicyclist turning 
movements should be pursued when dedicated bike infrastructure is not present. Site-level examination 
should be given to locations along well-traversed routes where a bicyclist must transition from a 
dedicated facility (e.g., bike lane), where SADR use is unauthorized, to sidewalks shared by pedestrians 
as well as SADRs and an ever-increasing share of other human pathway users (e.g., e-scooters). 
Regarding the grouping of SADR-bicyclist interactions near bike parking facilities—visualized in Figure 5, 
positioning of SADR delivery points away from main building entrances with nearby bike racks or bike 
lockers to alternative building entryways should be prioritized to reduce SADR-bicyclist interactions. The 
introduction of designated SADR delivery stations delineated by physical path markings and warning 
signs as a curb management strategy would provide further information to nearby bicyclists about the 
presence of SADRs in the area. 
This real-world analysis of SADR conflicts with pedestrians and bicyclists also underscored a need for 
future changes in the design of self-driving food delivery devices and the development of warning signs 
noting their presence. The identification of SADR-human pathway user interactions categorized as 
dangerous conflicts—including 12 interactions with a PET value of zero seconds—confirms that these 
new autonomous technologies are disrupting travel as a pedestrian or bicyclist despite any initial best 
effort by SADR service providers to seamlessly introduce these autonomous devices on pathways 
designed for our transportation system’s most vulnerable users. In response, design advancements to 
SADRs that improve their visual and audible detection by human pathway users should be considered. 
Presently, Starship robots produce a ‘chirping’ noise when close to a human pathway user. However, in 
areas of higher traveler volumes or those with a confluence of sidewalks and shared-use paths, this 
audible cue may be given too late for an approaching bicyclist, e-scooter rider, or wheelchair user 
traveling faster than a pedestrian to make any required evasive maneuver. Accordingly, the instruction 
for SADRs to generate audible cues when traveling in designated high-activity zones identified by 
contracting partners or via built-in detection sensors may offer safety benefits to a wider range of 
human pathway users. As a complement, caution or warning signs should be introduced to alert human 
pathway users to the increased potential for interaction with SADRs in these high-activity zones. 
Although the development of warning signs is likely to reduce possible SADR conflicts with active 
transportation adopters, public agencies seeking to introduce emerging or novel methods for last-mile 
food deliveries should be cognizant of placing further hardships on pedestrians or bicyclists who at 
present face increased constraints on safe travel in urban settings due to the popularity of ridesourcing 
and food delivery services on roadways and new micromobility options on already-crowded sidewalks. 
Accordingly, as SADR technology providers look to expand their markets beyond university campuses to 
meet a growing demand for online food delivery services, attention should be given to vehicle design 
improvements that increase their detection across a more heterogenous population and built 
environment. These design considerations should include the requirement of greater lighting to meet 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
26 
 
the likelihood of SADRs traveling during evening hours or inclement weather conditions as well as 
alterations to vehicle profiles to both meet the likely increased demand for larger food deliveries (e.g., 
groceries) and improve their visibility to human pathway users who may not easily detect a vehicle that 
is less than two feet in height within their traveling sightlines. 
Study conclusions 
This study represents an early investigation into the impacts of autonomous food delivery robots sharing 
pathways with pedestrians and bicyclists, with its findings providing evidence on the traffic safety 
conditions experienced by active travelers interacting with SADRs in a real-world setting. An immediate 
contribution of this study is its offering of new insights to planners, engineers, and policymakers who 
seek facility management strategies capable of supporting the safe introduction of this emerging 
autonomous freight technology on shared-use facilities in urban settings. Potentially successful 
mitigation strategies can be derived from this study’s description of SADR-involved interactions with 
pedestrians and bicyclists, which found that moderate and dangerous conflicts cluster near sites with 
intersecting and narrow pathways without any delineation of what space travelers should occupy. 
Statistical model results found the PET-measured severity of SADR-involved interactions with active 
travelers tended to increase when an SADR crossed a human pathway user’s intended trajectory, with a 
pedestrian or bicyclist often altering their path to avoid any collision. Findings suggest the safe 
introduction of SADRs onto urban pathways and curb spaces will be a challenge for practitioners that is 
likely to require innovative solutions in SADR route programming, public education, and infrastructure 
design. 
Methodological contributions aimed at guiding future research using SSMs to understand traffic safety 
conditions attributed to autonomous devices without well-defined travel lanes are also made with this 
study. To identify SADR-human pathway user interactions, this study established an adapted PET metric 
that requires the creation of a site-level, static bounding box and a user-level, dynamic conflict area. The 
former geography is customary to PET analyses exploring vehicle-based interactions, which are generally 
contained within delineated travel lane(s), while the second geography operates as a nested bounding 
box capable of defining incidents involving pathway users with physical dimensions narrower than their 
travel lane(s) or which travel on shared-use facilities. This study’s adaptation of an existing SSM to 
explore how smaller emerging autonomous freight delivery devices interact with pedestrians and other 
sidewalk users could foreseeably be transferred to study the possible impacts that new road 
autonomous delivery robots would have on motor vehicles when traveling on facilities designed for use 
by the latter user type. 
While this study offers contributions toward improving any present understanding of how traffic safety 
conditions for pedestrians and bicyclists are being altered via the introduction of SADRs, there are 
notable study limitations that should be addressed with future research. First, while the introduction of 
Starship’s SADR fleet to NAU’s main campus provided a real-world setting to undertake this research, 
the landscape and traveler composition attributed to a college campus does not represent the urban 
context or general population that is likely to experience any future, large-scale deployment of 
autonomous food delivery services. Second, although this study’s video data collection effort generated 
a mostly balanced distribution of SADR-involved interactions across severity levels, the sample had 
fewer dangerous conflicts than the other two interaction categories, which limited the statistical power 
of the modeling. Finally, the study sample of SADR-involved interactions removed any conflict initiated 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
27 
 
by a human pathway user; however, some pedestrians and bicyclists may have greater comfort traveling 
in proximity to SADRs. Any such individual-level comfort variation is likely to skew the sample toward 
interactions with a lower, non-zero PET and greater observed severity, and cannot be fully 
comprehended without an investigation into how different market segments react to the introduction 
of SADRs on pathways shared by pedestrians and bicyclists. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
28 
 
Study 2: Reported Pedestrian and Bicyclist Comfort in Sharing 
Pathways with Sidewalk Autonomous Delivery Robots 
Background 
The growth and evolution of e-commerce, spurred by advancements in information and communication 
technology and public health conditions necessitating contactless delivery services to help mitigate 
Covid-19 transmission, has driven the demand for app-based automated delivery robots capable of 
transporting meals to customers in a safe and efficient manner (18;19). Though the real-world 
implementation of road autonomous delivery robots as a last-mile food or grocery delivery service 
remains in its pilot stage (20), the small-scale deployment of sidewalk autonomous delivery robots 
(SADR) has recently occurred in limited settings. In the United States, SADR deployment for food and 
drink deliveries has been primarily confined to university campuses and operated by Starship 
Technologies or Kiwibot, each of which have partnered with campus dining provider, Sodexo. Starship 
launched their autonomous delivery robots at George Mason University and Northern Arizona 
University (NAU) in early 2019 (2), while Kiwibot prototyped their low-speed automated delivery vehicle 
at the University of California, Berkeley one year prior (21). In Spring 2022, Kiwibot was operating its 
SADR food delivery service across 10 US college campuses (22) and Starship had SADR fleets traversing 
18 campuses (23). 
An expansion of automated delivery robot services across college campuses is largely motivated by the 
abundance of pedestrian paths and limited vehicular traffic found at these sites as well as a captive 
market of young, technology savvy adults with on-demand food delivery service experience (24). 
Meanwhile, US college students are also more likely to adopt sustainable travel modes (25) and the 
introduction of SADRs to shared-use paths designed for pedestrians and bicyclists that have witnessed 
an increase in micromobility-related conflicts (26) is likely to create new traffic safety concerns for active 
travelers on college campuses. However, to-date, limited evidence exists on how these emerging last-
mile delivery services may be impacting pedestrian and bicyclist travel decisions or patterns in a real-
world setting of SADR deployment despite the continued proliferation of these services across US 
college campuses and early efforts to expand into city neighborhoods. 
In response, this study describes the design and administration of an original survey instrument to a 
campus population with experience adopting this new freight technology and traversing the shared-use 
pathways in which it operates. Specifically, this study seeks to (i) generate new evidence describing 
experiences with and future intentions to use automated food delivery services of a younger 
demographic who is more likely to be early adopters of new technologies and (ii) identify personal 
attributes and SADR-related experiences or intentions that are associated with self-reported comfort in 
sharing pathways with this new last-mile freight technology from the perspective of a pedestrian or 
bicyclist. In all, study findings are intended to offer new information for transportation planners and 
policymakers about the adoption of and safety concerns related to SADR fleet introduction and guidance 
on facility management strategies or design considerations that may facilitate a safe, future introduction 
of SADRs to more urban landscapes. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
29 
 
Literature review 
The development of innovative last-mile food delivery technologies has become ever more important as 
the continued use of conventional car and driver methods (e.g., UberEats, DoorDash) to meet the 
growing demand for instantaneous meal deliveries has exacerbated urban traffic congestion. 
Autonomous delivery vehicles that use pathways shared by pedestrians, bicyclists, and new 
micromobility travelers have been introduced in limited settings as a possible future solution for more 
efficiently delivering food and drinks to technologically-inclined consumers. While some research has 
been conducted to assess the viability of SADRs as an effective food or parcel delivery service via an 
evaluation of existing legislation and technical capabilities that permit their operation or potential time 
cost savings that they may generate (3), any study of their performance in a real-world setting has been 
limited. Prior to the onset of the Covid-19 pandemic, last-mile package delivery research largely focused 
on identifying improvements in delivery route efficiency via computer-based automation (19). Yet, with 
the rise in public health concerns related to infectious disease transmission, interest in SADR-related 
studies has increased due to the intrinsic ability of robot delivery fleets to limit person-to-person contact 
(20) has led to a handful of recent studies investigating the willingness of certain markets to potentially 
adopt low-speed automated delivery services. 
In an early study of SADRs as a last-mile delivery solution, Jennings and Figliozzi (3) examined the 
regulatory environment and technical capabilities of this new automated freight service to model their 
travel impacts in a generalized setting. The results from this study indicated that SADRs could potentially 
provide substantial cost and time savings under certain conditions and greatly reduce on-road travel per 
package delivery (3). In a second study, Boysen et al. (27) evaluated applications of truck-based SADR 
fleets in which a van transports smaller SADRs to a central depot where these devices are launched to 
autonomously deliver goods to nearby consumers, finding the use of decentralized robot depots may 
contribute to a more efficient parcel delivery process. Simoni et al. (28) also assessed the efficiency of 
truck-based SADR fleets to complete last-mile package deliveries by identifying a solution to a special 
case of the weighted interval scheduling problem that indicates robot-assisted delivery systems can be 
efficiently deployed in congested areas if the robots are fitted to hold multiple deliveries at once. 
Another hypothetical application of a popular vehicle routing problem was put forth by Chen et al. (19) 
to investigate the potential challenges and benefits of implementing self-driving delivery robots in cities, 
with study findings showing that their new heuristic algorithm could generate highly effective instances 
for managing demand from up to fifty consumers. By incorporating a set of SADR delivery route features 
such as sidewalk width, surface condition, presence of driveways and crosswalks, and route length, 
Corno and Savaresi (29) developed the Sidewalk Robot Feasibility Index (SRFI) as a new measure of SADR 
route feasibility along urban pathways. 
While these reviewed studies centered on improving the efficiency and feasibility of SADR routes and 
systems, fewer studies have sought to identify consumer acceptance of automated delivery robots. 
Kasper and Abdelrahman (18) designed and administered a four-part survey instrument with questions 
using validated scales to investigate the user acceptance of automated delivery vehicles in Germany via 
an extension of the Unified Theory of Acceptance and Use of Technology. In analyzing the results of this 
online survey with 501 respondents, the authors revealed that price sensitivity was the strongest 
indicator of user acceptance but noted that most respondents were unfamiliar with this new technology 
and were solely reliant on the provided information sheet and their imagination (18). For a Portland, 
Oregon study of consumer attitudes toward autonomous delivery robot service adoption, Pani et al. (20) 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
30 
 
recruited a representative sample of 483 consumers using a four-part questionnaire eliciting information 
about their socioeconomic characteristics, shopping perceptions, mobility tools, and attitude changes 
around the Covid-19 pandemic. In this latter study, the authors identified six latent clusters that were 
then associated with a respondent’s willingness to pay for autonomous delivery robot services, with 
findings suggesting that younger individuals and those with higher educational attainments and incomes 
were more likely to pay for contactless package deliveries (20). 
Based on this reviewed literature, it is evident that research on SADRs or other automated delivery 
robot services has largely focused on improving the efficiency of their operation, with limited study of 
the demand consumers have for this technology. In fact, only a pair of studies were identified by the 
authors, which examined consumer preferences for SADRs and neither of those survey efforts were 
administered to a population with first-hand experience with their operation. Furthermore, there were 
no studies found by the authors that examined the safety impacts of operating SADRs on transportation 
facilities shared by human pathway users (e.g., pedestrians, bicyclists). This study’s design and 
subsequent administration of an original survey instrument aimed at revealing SADR adoption patterns 
and identifying the traffic safety concerns attributed to their deployment in a real-world setting helps to 
address recognized gaps in this new research area and offer needed guidance to transportation planners 
and policymakers on the topic. 
Methods 
Survey instrument and administration 
The Perceptions of Autonomous Robots for Deliveries (PAR-D) survey instrument (Appendix A) was 
designed to identify the perceptions of and experiences with SADRs shared by NAU students, faculty, 
and staff; a community with three years of real-world knowledge pertaining to the adoption of and 
interaction with SADR services. The design of the tablet-based PAR-D survey instrument in Qualtrics 
software was reviewed and approved by the Institutional Review Board of NAU (Project #1891635-1). 
The survey instrument consisted of three components and 20 questions and was designed to take a 
participant less than five minutes to complete. In the first section (Survey Participant Information), 
survey participants were asked to answer questions regarding their sociodemographic and economic 
background and general travel behaviors. Responses to these questions, which provided information on 
a survey respondent’s self-reported age, gender, race or ethnicity, income, employment status, and 
educational attainment as well as information regarding their residential location and common travel 
modes, were collected to help illustrate the characteristics of this study’s sample associated with recent 
and future SADR utilization. 
The survey instrument’s second section (Autonomous Delivery Vehicles) sought to collect information 
on the reported experiences of participants with SADRs operating on the NAU campus, their perceptions 
regarding comfort for sharing pathways with SADRs as a pedestrian or bicyclist, their intentions of 
adopting autonomous food delivery services in the future, and whether autonomous food delivery 
vehicles would operate best on pathways exclusive to pedestrians and bicyclists or roadways with 
motorists. Responses to these last three items were recorded using a five-point Likert scale ranging from 
least to most agreement. The aforementioned question on SADR use employed seven ordered 
categories that ranged from ‘never’ to ‘one or more times per day’, while two questions on comfort in 
sharing pathways with SADRs as a pedestrian or bicyclist were paired with five-point Likert scales that 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
31 
 
ranged from ‘very uncomfortable’ to ‘very comfortable’, with an option to state they ‘don’t know’ or in 
the case of the latter mode, ‘don’t ride a bicycle’. 
The final section of the PAR-D survey instrument presented a stated choice experiment in which 
participants were presented with four five-second field-recorded videos showing a head-on conflict with 
an SADR from the perspective of a pedestrian or bicyclist. Two videos were shown for each active 
traveler type, with the pedestrian or bicyclist taking a lateral evasive maneuver within two differing 
times before a possible SADR-involved collision. The four videos were recorded in a controlled setting on 
NAU’s campus, with the evasive action of the pedestrian or bicyclist made at approximately one or three 
seconds before an SADR collision would occur. The decision to record videos with these evasive action 
times was informed by applying a surrogate safety measure, post-encroachment time (PET), which has 
been used recently in the study of active traveler safety in public shared spaces (7). PET is defined as the 
time elapsed from the moment when a vehicle (or other pathway user) departs a potential collision site 
to the moment of arrival at the potential collision site by a conflicting vehicle or other pathway user (6), 
with a smaller PET value reflecting a more-severe conflict and a value of zero seconds indicating a 
collision. The recording of videos in this study’s choice experiment with PET values of approximately one 
and three seconds, representing a dangerous and moderate SADR-involved conflict, respectively. Figure 
6 provides screen shots from two of the videos showing dangerous conflicts between an SADR and 
pedestrian (left image) and SADR and bicyclist (right image). After viewing each video, survey 
participants, who were given the ability to replay the field recording, were asked to indicate their 
comfort in sharing the pathway with an autonomous delivery vehicle by using an ordered five-point 
scale: Very uncomfortable, uncomfortable, neutral, comfortable, or very comfortable. 
Figure 6. Video screen shot of SADR conflicts in stated choice experiment with pedestrian (left) and 
bicyclist (right) 
 
 
For this study, the PAR-D survey instrument was administered to a sampling population of NAU 
students, faculty, and staff at the two campus unions where Starship SADRs regularly depart to offer 
food and drink deliveries to on-campus buildings and residences. Intercept survey responses were 
collected by a team of three survey administrators during two weeks in April 2022, when in-person 
classes were in session. Each survey administrator was equipped with a portable tablet device 
connected to Wi-Fi services, which they presented to a prospective survey participant with a brief 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
32 
 
prompt inquiring about their willingness to take a five-minute survey about their individual perceptions 
of Starship robots. In total, 526 survey responses were collected inside both the University Union on 
north campus and the du Bois Center on south campus, with four responses removed from the final 
study sample due to incompleteness. 
Statistical analysis 
Using this primary data set, a statistical analysis was conducted to understand how information 
collected on a survey respondent’s socioeconomic background, travel behaviors, and experiences and 
perceptions regarding SADR utilization and pathway interactions relates to their reported comfort in 
sharing pathways with SADRs as a pedestrian or bicyclist. The outcome variables for this analysis are the 
responses received to the four stated choice experiments, as a function of variables constructed from 
responses to questions in the PAR-D survey instrument’s first two sections. Provided the ordered nature 
of the outcome variables, an ordered logistic regression analysis was pursued. However, the relatively 
small sample size of this study (n=522) and the likely prospect for existing differences in mode-specific 
responses related to comfort (i.e., walking is more common than bicycling by the campus population) 
necessitated a review of the ordered logistic regression’s assumption of proportional odds. In addition 
to ensuring that a balanced distribution of survey responses was tallied for each response category, any 
aggregation of the original responses to the stated choice experiments needed to be identical across the 
two SADR-pedestrian and SADR-bicyclist questions. As a result, the outcome variable for the two models 
of pedestrian-related comfort in evading a possible SADR conflict recorded PET values of one and three 
seconds, respectively, were aggregated to four levels, where: 0 = Very uncomfortable or uncomfortable, 
1 = Neutral, 2 = Comfortable, and 3 = Very comfortable. A further aggregation of the original response 
categories was made for outcome variables in the two models of bicyclist-related comfort in evading a 
possible SADR conflict due to limited responses of ‘comfortable’ or ‘very comfortable’, resulting in the 
following three levels: 0 = Very uncomfortable or uncomfortable, 1 = Neutral, 2 = Comfortable or very 
comfortable. 
Proceeding with these adjusted responses, the four ordered logistic models specified in this study can 
be expressed in Equation 2 (17), which is an extension of a logistic regression model applied when a 
dependent variable is an ordered-response with two or more discrete levels: 
𝑃(𝑦𝑖 > 𝑗) =
𝑒𝑥𝑝(𝑋𝑖𝛽′−𝜙𝑗)
1+𝑒𝑥𝑝(𝑋𝑖𝛽′−𝜙𝑗) 𝑗 = 1, 2, … , 𝑀 − 1 (2) 
 
where j is the self-reported comfort level, X_i is a vector of revealed survey respondent attributes, β is a 
vector of estimated parameters, ϕ_j are the breakpoints associated with the comfort level thresholds, 
and M is the number of categories of the ordered-response variables for SADR-pedestrian and SADR-
bicyclist conflicts after employing the above aggregation process. 
Each of the four ordered logistic regression models constructed for this analysis of active traveler 
comfort in evading an approaching SADR on a shared-use pathway was specified via the following 
process. Initially, unadjusted Spearman’s Rank correlation coefficients were calculated for each survey 
respondent attribute and SADR-related responses to items in the survey instrument’s second section. 
Predictors with a marginally significant association (p<0.10) were identified and added to a full model 
specification. Then, beginning with the full model specification, a backwards elimination process was 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
33 
 
performed to remove non-significant predictors from a final model specification. This iterative process 
of independent variable removal was performed by dropping the predictor with the highest, non-
significant p-value (p>0.10) until a final specification was reached where all remaining independent 
variables were significant predictors of the ordered outcome variable. 
Data and results 
Survey respondent profile 
In all, 522 valid responses were received from the two-week administration of the PAR-D survey on 
NAU’s campus. Table 5 provides a summary of the sociodemographic and economic characteristics of 
the final study sample in addition to information on modes adopted for travel to and around the 
university campus. Nearly three out of four survey respondents stated that their residence was one of 
the on-campus student housing options. Among the survey respondents who noted residing off-campus 
(n=131), approximately two-fifths (41%) of respondents noted a car was their most common means of 
traveling to NAU. In turn, about one quarter (24%) of off-campus survey respondents stated that they 
commonly walked to campus, with the remainder of off-campus respondents either primarily riding 
public transit (20%) or cycling (10%) when traveling to campus. While at campus, a vast majority of all 
survey respondents noted that walking (85%) was one way they traveled around campus, with about 
one half (48%) of students traversed campus on a bus. Less than one third (32%) of survey respondents 
noted that they traveled via car around campus, while only 16% of survey respondents noted riding their 
private bicycle or one provided by the university’s shared Yellow Bike Program around campus. 
 
Table 5. Descriptive statistics of PAR-D survey respondents 
Variable Count (n) Share (%) 
Residence: On-campus 391 74.90 
Residence: Off-campus 131 25.10 
Gender: Male 226 43.29 
Gender: Female 272 52.11 
Gender: Non-binary or self-describe 24 4.60 
Age: 18-24 years old 503 96.36 
Age: 25-34 years old 16 3.10 
Age: 35 years old or more 3 0.58 
Education: High school or less 59 11.30 
Education: Bachelors or some college 460 88.12 
Education: Masters of PhD 3 0.58 
Race/Ethnicity: American Indian or Alaska Native 9 1.73 
Race/Ethnicity: Asian 26 5.01 
Race/Ethnicity: Black/African American 13 2.50 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
34 
 
Race/Ethnicity: Hispanic/Latinx 65 12.52 
Race/Ethnicity: Multiple races or ethnicities 47 9.06 
Race/Ethnicity: Native Hawaiian or Pacific Islander 6 1.16 
Race/Ethnicity: White, Non-Hispanic 353 68.02 
Personal Income: Less than $15,000 435 90.06 
Personal Income: $15,000-$34,999 42 8.70 
Personal Income: $35,000-$49,999 2 0.41 
Personal Income: $50,000 or more 4 0.83 
Work status: Part-time student 13 2.58 
Work status: Full-time student 388 75.19 
Work status: Part-time employment 186 36.05 
Work status: Full-time employment 18 3.49 
Travel mode: To campus (car) 54 54.55 
Travel mode: To campus (walk) 32 32.32 
Travel mode: To campus (bike) 13 13.13 
Travel mode: Around campus (car) 165 31.67 
Travel mode: Around campus (walk) 444 85.22 
Travel mode: Around campus (bike) 81 15.55 
 
 
Regarding the sociodemographic and economic attributes of the study sample, most survey respondents 
identified their gender as female (52%), with the remainder of survey respondents to this question 
stating they were male (43%) or non-binary (5%). In terms of survey respondents’ race and ethnicity, 
about two thirds (68%) of survey respondents in the final study sample identified as White, Non-
Hispanic, with 13% of respondents identifying as Hispanic or Latinx and 9% describing themselves as 
multiracial or multiethnic. To be expected when surveying a university population, the age distribution 
across the study sample was heavily skewed and not representative of a general population, with 96% of 
survey respondents between 18 and 24 years of age. Accordingly, the educational attainment response 
for the study sample almost entirely fell into the categories encompassing a high school diploma or 
some college (99%). Not shown in Table 1, of the 401 survey respondents who were enrolled as a part- 
or full-time student, about one half (48%) of those students were Freshman. This overrepresentation in 
the sample is attributable to a decision to administer the survey in student unions with assorted meal 
options and the requirement of the university for all first-year students living on-campus to purchase an 
annual meal plan. While many survey respondents were currently students at NAU, 39% of respondents 
stated they were also part- or full-time workers. Though, the most popular personal income cohort 
found in the study sample reflected an annual income less than $15,000. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
35 
 
The PAR-D survey instrument’s second component sought to identify experiences of survey respondents 
with SADRs in a real-world setting and to understand their perspectives regarding a potential future 
wide-scale deployment of low-speed automated freight vehicles. Table 6 summarizes the results from 
this part of the survey instrument. When asked about the frequency at which survey respondents use 
Starship robot delivery services to order meals or drinks, which cost $1.99 per delivery, about three out 
of four (76%) survey respondents stated they had adopted this delivery service at least once in the past. 
Of those 393 respondents with experience using Starship delivery services, there was a well-balanced 
distribution of individuals who had used the service less than once per month over the past year (49%) 
and those who adopt this service at least twice per month (51%). Moreover, almost one quarter (23%) 
of sampled individuals who had adopted Starship delivery services in the past stated they utilize this 
service once a week or more. Meanwhile, 36% of survey respondents agreed (level 4) or strongly agreed 
(level 5) with a five-point Likert scale statement about the future adoption of autonomous delivery 
vehicles as a food or grocery delivery option, with disagreement (level 2) or strong disagreement (level 
1) from 34% of respondents and a neutral (level 3) response (29%) from the remainder of respondents. 
 
Table 6. Descriptive statistics of PAR-D survey responses regarding SADRs 
Survey Instrument Item and Response Count (n) Share (%) 
How often do you use Starship robot delivery services to order food or drinks? 
 Never 126 24.14 
 Very rarely (one time per year or less) 87 16.67 
 Rarely (one time per month or less) 108 20.69 
 Occasionally (two or three times per month) 109 20.88 
 Frequently (one time per week) 51 9.77 
 Very frequently (two or more times per week) 30 5.74 
 Always (one or more times per day) 11 2.11 
As a PEDESTRIAN or a BICYCLIST, have you altered your intended path because of an interaction with an 
autonomous Starship robot delivery service? 
 Yes 371 71.49 
 No 132 25.43 
 Don’t know 16 3.08 
As a PEDESTRIAN, what is your comfort in sharing pathways with autonomous robot delivery services? 
 Very uncomfortable 36 6.90 
 Uncomfortable 25 4.79 
 Neutral 141 27.00 
 Comfortable 169 32.38 
 Very comfortable 150 28.74 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
36 
 
 Don’t know 1 0.19 
As a BICYCLIST, what is your comfort in sharing pathways with autonomous robot delivery services? 
 Very uncomfortable 28 5.36 
 Uncomfortable 57 10.92 
 Neutral 108 20.69 
 Comfortable 44 8.43 
 Very comfortable 31 5.94 
 Don’t know or don’t ride a bicycle 254 48.66 
I intend to use autonomous delivery vehicles as a food or grocery delivery option. 
 1 (Least agreement) 100 19.16 
 2 80 15.32 
 3 152 29.12 
 4 127 24.33 
 5 (Most agreement) 63 12.07 
Autonomous delivery vehicles will work well if sharing pathways with only pedestrians and bicyclists. 
 1 (Least agreement) 28 5.38 
 2 84 16.12 
 3 195 37.43 
 4 131 25.14 
 5 (Most agreement) 83 15.93 
Autonomous delivery vehicles will work well if sharing roadways with only motorists. 
 1 (Least agreement) 164 31.48 
 2 175 33.59 
 3 123 23.61 
 4 33 6.33 
 5 (Most agreement) 26 4.99 
 
 
Comfort in sharing pathways with SADRs 
Administration of the PAR-D instrument also provided insights into the comfort that survey respondents 
expressed for sharing pathways with SADRs as active travelers on NAU’s campus. About one half of 
survey respondents stated they were either comfortable (32%) or very comfortable (28%) with sharing 
pathways with SADRs, with 11% expressing that they were either uncomfortable or very uncomfortable. 
Over three quarters (76%) of female survey respondents noted that they were comfortable or very 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
37 
 
comfortable with sharing pathways with SADRs, while a fewer percentage (61%) of male survey 
respondents stated a similar level of comfort in sharing pathways with autonomous robot delivery 
services as a pedestrian (Table 7). In terms of travel modes, 63% of survey respondents who primarily 
walk to campus and 60% of survey respondents who walk around campus stated that they were 
comfortable or very comfortable in sharing pathways with SADRs as a pedestrian. While only a fraction 
(13%) of survey respondents rides a bicycle to campus, 27% of those respondents who ride a bicycle 
expressed that they were comfortable or very comfortable in sharing pathways with SADRs. In contrast 
to the reported comfort in sharing pathways with SADRs as a pedestrian, only 32% of female survey 
respondents noted that they were comfortable or very comfortable with sharing pathways with SADRs 
as a bicyclist, while about one quarter (24%) of male survey respondents stated a similar level of 
comfort. Furthermore, more than two out of five (41%) survey respondents who ride a bicycle around 
campus reported being uncomfortable or very uncomfortable with sharing pathways with SADRs as a 
bicyclist. 
 
Table 7. Reported comfort sharing pathways with SADRs by select variables 
What is your comfort sharing 
pathways with autonomous robot 
delivery services? 
As a PEDESTRIAN As a BICYCLIST 
Variable VU U N C VC VU U N C VC 
Gender: Male 13 11 62 85 55 14 40 55 23 11 
Gender: Female 23 14 73 78 92 14 17 48 19 19 
Gender: Non-binary/self-describe 0 0 6 6 3 0 0 5 2 1 
Travel mode: To campus (car) 4 2 12 18 18 1 3 14 3 2 
Travel mode: To campus (walk) 3 1 8 7 13 0 3 6 3 6 
Travel mode: To campus (bike) 0 0 6 4 3 1 6 5 1 0 
Travel mode: Around campus (car) 17 10 37 49 52 10 8 29 17 9 
Travel mode: Around campus 
(walk) 29 21 125 143 125 24 47 87 34 23 
Travel mode: Around campus (bike) 3 2 25 32 19 7 26 26 16 6 
As a PEDESTRIAN or a BICYCLIST, have you altered your intended path because of an interaction with an 
autonomous Starship robot delivery service? 
Response VU U N C VC VU U N C VC 
 Yes 21 21 116 123 89 20 51 83 26 14 
 No 14 3 19 36 60 7 4 22 15 17 
 Don’t know 1 1 5 8 1 1 1 1 3 0 
Autonomous delivery vehicles will work well if sharing pathways with only pedestrians and bicyclists. 
Response VU U N C VC VU U N C VC 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
38 
 
 1 (Least agreement) 7 6 9 3 3 6 5 4 1 2 
 2 3 9 38 25 9 7 14 17 3 3 
 3 12 4 56 79 44 7 26 51 14 7 
 4 7 2 28 43 50 6 8 20 16 5 
 5 (Most agreement) 7 4 9 19 44 2 4 16 10 14 
Autonomous delivery vehicles will work well if sharing roadways with only motorists. 
Response VU U N C VC VU U N C VC 
 1 (Least agreement) 16 11 44 41 52 14 21 27 6 10 
 2 6 6 59 67 36 5 26 37 17 8 
 3 7 6 32 41 37 6 8 29 10 9 
 4 3 2 3 12 13 1 0 8 8 1 
 5 (Most agreement) 4 0 3 7 12 2 2 5 3 3 
 
 
The last pair of questions in the survey instrument’s second component applied the previously described 
five-point Likert scale to understand what types of transportation facilities would be most suitable for 
the operation of autonomous delivery vehicles. Again, about two out of five (41%) survey respondent 
believed that autonomous delivery vehicles will work well if sharing pathways with only pedestrians and 
bicyclists, which is currently the circumstance at NAU’s main campus, while only 11% of survey 
respondents agreed (level 4 or level 5) with the statement that autonomous delivery vehicles would 
work well if sharing roadways with only motorists. Looking at respondents who noted being comfortable 
or very comfortable in sharing pathways with SADRs as a pedestrian, about one half (49%) of those 
survey respondents agreed (level 4) or strongly agreed (level 5) that autonomous delivery vehicles will 
work well if sharing pathways with only pedestrians and bicyclists. Moreover, just 17% of survey 
respondents who reported being comfortable or very comfortable in sharing pathways with SADRs as a 
bicyclist agreed or strongly agreed that autonomous delivery vehicles will work well if sharing pathways 
with only pedestrians and bicyclists. However, few survey respondents who stated they were either very 
uncomfortable or uncomfortable in sharing pathways with SADRS as a pedestrian (15%) or bicyclist (6%) 
agreed (level 4) or strongly agreed (level 5) that autonomous delivery vehicles would work well if sharing 
pathways with only motorists. Taken together, the descriptive results highlighted in Table 3 appear to 
illustrate that pedestrians tended to be comfortable sharing facilities with SADRs but that those who 
were comfortable are split on how well SADRs operate on pathways shared with pedestrians and 
bicyclists. Bicyclists, in turn, have less comfort sharing pathways with these low-speed autonomous 
delivery vehicles but do not necessarily believe that this new technology will operate better in traffic 
with only motorists (e.g., roadways) than on facilities already shared by pedestrians and bicyclists. 
Results from the final component of the PAR-D survey instrument, which was a choice experiment 
where survey participants were shown four five-second videos of an SADR interaction from the 
perspective of a pedestrian or bicyclist, were then analyzed by specifying four ordered logit models. The 
outcome in each model was the self-reported comfort that a survey respondent would have for sharing 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
39 
 
a pathway with an SADR after viewing each video. Figure 7 shows the distribution of responses for 
participants who viewed the two videos from the perspective of a pedestrian making a lateral evasive 
maneuver out of the path of an oncoming SADR within one and three seconds, respectively, of a 
potential collision. Overall, the results illustrate that survey respondents tended to be comfortable in 
either scenario but were more likely to report a neutral comfort level after viewing the video with the 
lower evasive maneuver time. The distribution of responses following the viewing of the two videos 
from the perspective of a bicyclist who was avoiding a potential collision with an SADR are shown in 
Figure 8. The reported comfort levels were lower for the bicyclist than the pedestrian, with a neutral 
response being the most popular selection for each evasive maneuver time and respondents more likely 
to report being uncomfortable rather than very comfortable or comfortable after viewing the video of 
an evasive action made by the bicyclist one second prior to a potential SADR-involved collision. 
Figure 7. Reported pedestrian comfort in sharing a pathway with an SADR from two stated choice 
experiments 
 
 
 
 
 
 
 
 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
40 
 
Figure 8. Reported bicyclist comfort in sharing a pathway with an SADR from two stated choice 
experiments 
 
 
Table 8 summarizes the results of models in which the reported comfort for making an evasive 
maneuver one or three seconds in advance of a pedestrian possibly colliding with an oncoming SADR 
was a function of responses to the first and second components of the survey instrument. Investigating 
the results of the model with an estimated PET value of three seconds, survey respondents who 
revealed being either very uncomfortable or uncomfortable in sharing pathways with SADRs as a 
pedestrian were less likely to be comfortable having to make the evasive maneuver portrayed in the 
choice experiment’s video, whereas survey respondents who expressed some level of comfort in sharing 
pathways with SADRS tended to be comfortable with making an evasive maneuver that was three 
seconds before a potential SADR-involved collision. Survey respondents who were more likely to agree 
with statements about their future intention to adopt autonomous food delivery vehicles or note these 
services would only operative effectively if they were to share pathways exclusive to pedestrians and 
bicyclists were also found to be more comfortable with the three-second evasive action made by the 
pedestrian. Each of these variables was also a significant predictor in the model of an SADR-pedestrian 
interaction with an estimated PET value of one second, with no change in their direction of association. 
In this second model, an individual who reported being very comfortable or comfortable with sharing 
pathways with SADRs as a bicyclist was also comfortable with the one-second evasive action taken by 
the pedestrian. Furthermore, respondents who revealed that they presently use Starship robot delivery 
services at least twice per month appeared to be more comfortable as a pedestrian sharing a pathway 
with an SADR in which they would need to change their walking path one second prior to a potential 
collision. Yet, survey respondents who stated that they had altered their trajectory in the past as a 
pedestrian or bicyclist in order to avoid a collision with an SADR were less likely to be comfortable with 
the one-second evasive maneuver made by the pedestrian in the second video. A model result which 
may reflect discomfort experienced by a survey respondent from a previous near-miss or collision with 
an SADR operating in a real-world setting and concern over a similar future occurrence. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
41 
 
 
Table 8. Ordered logit model results for pedestrian conflict with an SADR 
Outcome: PET = ~3 seconds PET = ~1 second 
Variable Beta SE CI: 
5% 
CI: 
95% Beta SE CI: 
5% CI: 95% 
SADR-related Responses 
Current SADR Utilization 
 Occasionally or more 0.32 0.18 0.02 0.62 
Altered Path as Pedestrian or Bicyclist -0.51 0.19 -0.81 -0.20 
Share Path with SADR as Pedestrian 
 Uncomfortable or Very Uncomfortable -0.96 0.32 -1.50 -0.43 -1.15 0.33 -1.70 -0.61 
 Comfortable or Very Comfortable 1.36 0.20 1.04 1.69 1.39 0.21 1.06 1.73 
Share Path with SADR as Bicyclist 
 Comfortable or Very Comfortable 0.41 0.25 <0.01 0.81 
Future Intention of SADR Utilization 0.27 0.07 0.15 0.38 0.17 0.07 0.05 0.29 
SADR Paths with Pedestrians and 
Bicyclists 0.27 0.08 0.13 0.41 0.35 0.09 0.21 0.50 
Intercepts 
 Threshold: 0 and 1 0.18 0.31 -0.08 0.38 
 Threshold: 1 and 2 1.68 0.31 2.14 0.38 
 Threshold: 2 and 3 3.68 0.35 3.70 0.41 
Model Summary 
 Number of observations 518 518 
 Log likelihood -611.19 -595.02 
 Log likelihood (constants only) -687.40 -697.38 
 Akaike information criterion 1236.38 1210.04 
 
 
Table 9, in turn, summarizes the estimation of two ordered logit models of a survey respondent’s level 
of comfort with having to take an evasive action to avoid a collision with an SADR as a bicyclist, as 
depicted in separate videos with PET values of three and one second, respectively. For the model of the 
less-severe SADR-bicyclist interaction, survey respondents who were very uncomfortable or 
uncomfortable sharing a pathway with an SADR as either a bicyclist or pedestrian were less likely to be 
comfortable with watching a bicyclist take an evasive maneuver three seconds prior to a possible SADR-
involved collision. Meanwhile, survey respondents who expressed comfort in sharing pathways with 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
42 
 
SADRs as a bicyclist or pedestrian were more likely to be comfortable with the recorded SADR-involved 
conflict from a bicyclist’s perspective. Individuals who recorded a greater level of agreement with the 
survey instrument’s statements regarding their intention to adopt autonomous food delivery services in 
the future and their belief that autonomous delivery vehicles would operate well if sharing pathways 
with only pedestrians and bicyclists or with only motorists (mutually exclusive questions) in the future 
were positively related to their comfort level after viewing the bicyclist-SADR conflict video with a PET 
value of approximately three seconds. However, those survey respondents who reported altering their 
paths in the past due to the presence of an SADR on their pathway were not as comfortable with the 
less-severe of the two bicyclist-SADR conflicts depicted in the choice experiment than their counterparts 
who did not report encountering this circumstance previously. 
 
Table 9. Ordered logit model results for bicyclist conflict with an SADR 
Outcome: PET = ~3 seconds PET = ~1 second 
Variable Beta SE CI: 
5% 
CI: 
95% Beta SE CI: 
5% CI: 95% 
Respondent Characteristics 
Travel mode: Around campus (car) 0.58 0.20 0.25 0.90 
Travel mode: Around campus (bike) -0.47 0.28 -0.93 -0.01 
SADR-related Responses 
Current SADR Utilization 
 Rarely or Very Rarely -0.45 0.19 -0.76 -0.14 
Altered Path as Pedestrian or Bicyclist -0.48 0.21 -0.83 -0.13 -0.76 0.21 -1.10 -0.42 
Share Path with SADR as Pedestrian 
 Uncomfortable or Very Uncomfortable -0.90 0.32 -1.43 -0.37 -0.63 0.31 -1.14 -0.13 
 Comfortable or Very Comfortable 0.59 0.22 0.23 0.96 
Share Path with SADR as Bicyclist 
 Uncomfortable or Very Uncomfortable -0.93 0.26 -1.36 -0.50 -1.16 0.29 -1.65 -0.69 
 Comfortable or Very Comfortable 1.91 0.34 1.37 2.50 2.19 0.33 1.67 2.75 
Future Intention of SADR Utilization 0.16 0.08 0.03 0.29 
SADR Paths with Pedestrians and 
Bicyclists 
0.23 0.10 0.07 0.38 0.39 0.09 0.24 0.54 
SADR Paths with Motorists 0.30 0.09 0.15 0.45 0.36 0.09 0.22 0.51 
Intercepts 
 Threshold: 0 and 1 -0.17 0.44 0.56 0.41 
 Threshold: 1 and 2 2.74 0.46 2.78 0.43 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
43 
 
Model Summary 
 Number of observations 518 518 
 Log likelihood -426.64 -447.74 
 Log likelihood (constants only) -527.23 -565.87 
 Akaike information criterion 873.29 917.48 
 
 
As for the model results pertaining to a respondent’s comfort with the more-severe bicyclist-SADR 
conflict, an individual was also less likely to be comfortable with the recorded one-second PET 
interaction if they had previously altered their walking or bicycling trajectory because of an SADR 
traveling on their pathway. Similarly, respondents who expressed being uncomfortable or very 
uncomfortable sharing pathways with SADRs as a pedestrian or bicyclist were less comfortable with the 
evasive action taken by the bicyclist in the one-second video to avoid an SADR collision. A negative 
association with this outcome variable was also estimated in relation to current SADR utilization, with 
respondents who have only used autonomous food delivery services rarely or very rarely in the previous 
year reporting less comfort having to take an evasive action within one-second of a possible SADR-
bicyclist collision. In contrast, respondents who were found to be either comfortable or very 
comfortable sharing pathways with SADRs as a bicyclist were comfortable with the one-second evasive 
action taken by the bicyclist in the second video. Aligned with model results of the prior video showing a 
PET value of three seconds, respondents in greater agreement regarding the potential for automated 
delivery services in the future to work well on facilities with only pedestrians and bicyclists or only 
motorists also tended to report greater comfort with the more-severe interaction of an SADR 
approaching an evading bicyclist. Interestingly, however, individuals who bicycle around campus as a 
means of travel were less likely to be comfortable with the video of a bicyclist evading the trajectory of 
an oncoming SADR one-second prior to a possible collision, while the opposite relationship held true for 
survey respondents who reported traveling around campus in a car. 
Study conclusions 
This study has sought to help advance a nascent evidence base on the adoption of emerging 
autonomous delivery robots and the possible safety impacts for pathway users who share their 
transportation facilities. While a handful of studies have investigated the acceptance of automated last-
mile delivery vehicles for different market segments, there has been less examination into the adoption 
of these services in settings where autonomous delivery robot fleets have been deployed. The design of 
an original survey instrument and its administration in this study to a younger, university population 
who may be more likely to adopt these services in the future (20) offered insights on the experiences 
and future intentions to adopt SADR services for a market segment with the real-world experience of a 
small-scale deployment of this last-mile freight delivery technology. 
Across this study’s sample of mostly university students, about three quarters (76%) of all survey 
respondents had adopted SADRs for food deliveries in the past year, with nearly one quarter of (23%) 
this subset of respondents stating they received SADR food deliveries at least once per week. These 
findings revealed a proclivity for this younger demographic to adopt SADRs and helped to substantiate 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
44 
 
the growing popularity of an autonomous delivery robot service that started only three years before this 
study’s data collection effort. Moreover, the prospect of these last-mile food delivery services to retain 
their successful market penetration appear to be optimistic given that roughly two-thirds (64%) of all 
survey respondents were either in agreement or neutral when asked if they intend to adopt 
autonomous delivery vehicles as a food or grocery delivery option if they become more common in 
public places in five years. Recognizing the survey was administered at dining locations, which may have 
resulted in some undercounting of high-activity SADR adopters who prefer at-residence food deliveries, 
these study findings illuminate a potential for the demand of autonomous delivery robots to expand 
their service areas beyond university campuses. 
An investigation of the impacts of SADRs to pedestrians and bicyclists, which would likely increase in 
frequency with expansions to autonomous delivery systems reliant on the shared-use of transportation 
facilities, represents a second study contribution. Descriptive results from an administration of the PAR-
D survey instrument to a university population with first-hand experience in the SADR deployment 
found that pedestrians tended to be more comfortable in sharing pathways with SADRs than bicyclists. 
However, model results revealed that individuals who stated being uncomfortable in sharing pathways 
with SADRs as a pedestrian or reported having to alter their intended path in the past due to an 
autonomous delivery robot operating on a sidewalk were less comfortable having to take evasive 
actions to avoid the trajectory of an oncoming SADR. Linking comfort in sharing transportation facilities 
to adoption experience, survey respondents who more frequently used autonomous food delivery 
services in the past tended to be more comfortable in taking evasive actions to avoid potentially more-
severe interactions with SADRs as either a pedestrian or bicyclist. Yet, in more-severe SADR-bicyclist 
interactions, mode-specific experience should be considered given that survey respondents who bicycle 
around campus were less comfortable having to swerve to avoid a potential SADR-involved collision 
than respondents who drive around NAU’s campus. 
Given this study’s findings and contributions, transportation policies and interventions addressing the 
likely introduction of SADRs to new settings and their expansion in current university settings should 
receive greater consideration. Foremost, goals to further motivate active transportation by improving 
the safe adoption of utilitarian walking and bicycling travel and increase sustainable travel mode shares 
may be further hindered if proactive measures are not taken to remedy those close SADR-involved 
interactions that will undoubtedly occur on many sidewalks in urban settings. If the current deployment 
of SADRs onto facilities shared by pedestrians, bicyclists, and emergent micromobility options (e.g., e-
scooters) remains, as was found preferable by this study sample, then the management of sidewalks 
and programming of SADR routes must ensure these new freight technologies do not impede safe and 
efficient active travel. The rollout of demonstration projects rather than any untested full-scale 
deployment of SADRs should be pursued to help identify safety concerns expressed by new market 
segments and pathways where SADR operation is less of an impediment to pedestrians and bicyclists, as 
this study found that pedestrians or bicyclists who had previously altered their intended trajectory 
because of an approaching SADR were less comfortable having to navigate SADR encounters in which an 
evasive action would be needed to avoid a potential collision. 
While this study contributes to behavioral and safety research on autonomous delivery robots, it is not 
without limitations. First, this study’s administration of the PAR-D survey instrument as an intercept 
survey of a university population produced a convenient sample that does not represent a more general 
population. Although the adoption of this sampling area ensured that most participants were familiar 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
45 
 
with SADRs, the sociodemographic and economic characteristics of the study sample are unlikely to 
reflect the composition of a dense, mixed-use setting that may likely be a target for future SADR fleet 
deployments. Relatedly, the videos in the survey instrument’s choice experiment were recorded in a 
controlled campus setting that is not likely to capture the complexity and nuance of interacting with an 
SADR as a pedestrian or bicyclist navigating an urban environment. Of note, although this study’s 
sampling strategy was devised to increase the number and completeness of survey responses to be 
analyzed, only 15% of the final study sample included respondents with experience traveling around 
NAU’s campus on a bicycle. Accordingly, future research should employ targeted sampling strategies 
toward bicyclists to better understand how this segment of active travelers may respond to a 
deployment of autonomous delivery robots on shared-use facilities. Finally, the responses recorded for 
this study are self-reported and thus may contain some measurement error introduced to its analysis. 
Although the administration of this study’s original survey instrument generated new knowledge as to 
how SADRs operating in a real-world setting may impact the safety conditions for pedestrians and 
bicyclists, the collection of observed SADR-involved conflicts could provide more objective and 
complementary evidence needed to inform new policies and interventions. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
46 
 
Conclusion 
The two studies described in this research report present early evidence on the observed and reported 
traffic safety conditions experienced by individuals interacting with SADRs on shared-use pathways. For 
the first study, field-recordings of SADRs operating in shared-use settings with pedestrians, bicyclists, 
and other human pathway users were collected and examined by adapting the surrogate safety measure 
of PET to identify the prevalence, location, and severity of SADR-involved interactions. Potential conflict- 
and site-level characteristics associated with the recorded severity of observed SADR-involved incidents 
were then defined by estimating an ordered logit model. Findings from this first study, which introduced 
the concept of a nested and dynamic conflict area to measure the PET for interactions involving vehicles 
with narrower dimensions than their travel lane, highlighted the challenges of safely introducing SADRs 
onto pathways and curb spaces shared by pedestrians and bicyclists who were noted as often having to 
alter their intended path to avoid a collision with these new low-speed automated delivery services. For 
the second study, a survey instrument with stated choice experiments aimed at detecting an individual’s 
self-reported comfort in sharing pathways with SADRs as a pedestrian or bicyclist was administered to a 
college population with experience in the real-world commercial deployment of SADRs. Findings from 
this second study, which identified various personal attributes associated with SADR adoption levels and 
linked these and other individual characteristics to a survey respondent’s perceived level of comfort for 
sharing pathways with SADRs as an active traveler, offered new insights on the future intention to adopt 
automated food delivery services and comfort in traveling alongside SADRs from a population with first-
hand knowledge on the small-scale deployment of this new last-mile food delivery service. 
Taken together, the research contributions from these two studies are intended to help inform future 
research on the adoption and operation of autonomous delivery robots and provide practitioners and 
policymakers an indication of how an emerging technology with a potential to improve freight mobility 
in urban settings may be adversely impacting the traffic safety conditions for sustainable travel modes. 
For researchers, this project’s first study put forth a methodology for adapting an established surrogate 
safety measure to characterize conflicts on shared-use facilities that can be reapplied to understand the 
traffic safety conditions attributed to new freight delivery or mobility options that are being deployed on 
facilities originally designed for other pathway users. Moreover, model results from the project’s second 
study that suggested revealed travel behaviors were associated with an individual’s stated comfort level 
in sharing a transportation facility with an automated delivery robot should be further investigated to 
recognize the psychological barriers these devices may have on motivating active travel mode adoption. 
For practitioners and policymakers, a recognition of conflicts between SADRs and human pathway users 
and reported pedestrian and bicyclist discomfort in sharing pathways with SADRs that was found in this 
research project’s first and second studies, respectively, have established a need for new transportation 
facility management strategies, refined routing programs, and potential revisions to the physical design 
of low-speed automated delivery services that ensure their safe introduction and continued operation 
on facilities originally designed for pedestrians, bicyclist, or other vulnerable roadway users. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
47 
 
References 
1. Starship. A revolution in local delivery. https://www.starship.xyz/company. Accessed July 21, 2022. 
2. Northern Arizona University (NAU). (2019, March 25). “Hello! Here is your delivery:” Starship robots 
roll onto NAU’s campus. Campus & Community. https://news.nau.edu/starship-
robots/#.Xlgxf6hKiUk. Accessed July 21, 2022. 
3. Jennings, D., Figliozzi, M. Study of sidewalk autonomous delivery robots and their potential impacts 
on freight efficiency and travel. Transportation Research Record, Vol. 2673, No. 6, 2019, pp. 317-
326. 
4. Johnsson, C., Laureshyn, A., De Ceunynck, T. In search of surrogate safety indicators for vulnerable 
road users: A review of surrogate safety indicators. Transport Reviews, Vol. 38, No. 6, 2018, pp. 765-
785. 
5. Allen, B.L., Shin, B.T., Cooper, P.J. Analysis of traffic conflicts and collisions (No. HS-025 846). 1978. 
6. Lord, D. Analysis of pedestrian conflicts with left-turning traffic. Transportation Research Record, 
Vol. 1538, No. 1, 1996, pp.61-67. 
7. Beitel, D., Stipancic, J., Manaugh, K., Miranda-Moreno, L. Assessing safety of shared space using 
cyclist-pedestrian interactions and automated video conflict analysis. Transportation Research Part 
D: Transport and Environment, Vol. 65, 2018, pp. 710-724. 
8. Liang, X., Meng, X., Zheng, L. Investigating conflict behaviours and characteristics in shared space for 
pedestrians, conventional bicycles and e-bikes. Accident Analysis & Prevention, Vol. 158, 2021. 
9. Chen, P., Zeng, W., Yu, G. Assessing right-turning vehicle-pedestrian conflicts at intersections using 
an integrated microscopic simulation model. Accident Analysis & Prevention, Vol. 129, 2019, pp. 
211-224. 
10. Ni, Y., Wang, M., Sun, J., Li, K. Evaluation of pedestrian safety at intersections: A theoretical 
framework based on pedestrian-vehicle interaction patterns. Accident Analysis & Prevention, Vol. 
96, 2016, pp. 118-129. 
11. Olszewski, P., Dąbkowski, P., Szagała, P., Czajewski, W., Buttler, I. Surrogate safety indicator for 
unsignalised pedestrian crossings. Transportation Research Part F: Traffic Psychology and Behaviour, 
Vol. 70, 2020, pp. 25-36. 
12. Stipancic, J., Zangenehpour, S., Miranda-Moreno, L., Saunier, N., Granié, M.A. Investigating the 
gender differences on bicycle-vehicle conflicts at urban intersections using an ordered logit 
methodology. Accident Analysis & Prevention, Vol. 97, 2016, pp. 19-27. 
13. Zangenehpour, S., Strauss, J., Miranda-Moreno, L.F., Saunier, N. Are signalized intersections with 
cycle tracks safer? A case–control study based on automated surrogate safety analysis using video 
data. Accident Analysis & Prevention, Vol. 86, 2016, pp. 161-172. 
14. Guo, Y., Sayed, T., Zaki, M.H. Exploring evasive action–based indicators for PTW conflicts in shared 
traffic facility environments. Journal of Transportation Engineering Part A: Systems, Vol. 144, No. 11, 
2018. 
15. Nikiforiadis, A., Basbas, S., Garyfalou, M.I. A methodology for the assessment of pedestrians-cyclists 
shared space level of service. Journal of Cleaner Production, Vol. 254, 2020. 
16. Russo, B.J., Kothuri, S., Smaglik, E., Aguilar, C., James, E., Levenson, N., Koonce, P. Exploring the 
impacts of intersection and traffic characteristics on the frequency and severity of bicycle-vehicle 
conflicts. Advances in Transportation Studies: An International Review. 2020, pp. 5-18. 
17. Long, J. S. (1997). Regression Models for Categorical and Limited Dependent Variables. Thousand 
Oaks, CA: Sage Publications. 
18. Kapser, S., Abdelrahman, M. Acceptance of autonomous delivery vehicles for last-mile delivery in 
Germany–Extending UTAUT2 with risk perceptions. Transportation Research Part C: Emerging 
Technologies, Vol. 111, 2020, pp. 210-225. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
48 
 
19. Chen, C., Demir, E., Huang, Y., Qiu, R. The adoption of self-driving delivery robots in last mile 
logistics. Transportation research Part E: Logistics and Transportation Review, Vol. 146, 2021. 
20. Pani, A., Mishra, S., Golias, M., Figliozzi, M. Evaluating public acceptance of autonomous delivery 
robots during COVID-19 pandemic. Transportation Research Part D: Transport and Environment, Vol. 
89, 2020. 
21. Kane, W. (2018, May 31). Those four-wheeled robots on campus, explained. Berkeley News. 
https://news.berkeley.edu/2018/05/31/those-four-wheeled-robots-on-campus-explained. Accessed 
July 28, 2022. 
22. Sodexo. (2022, February 15). Kiwibot finishes Pre-Series A funding and closes the biggest expansion 
deal with Sodexo. https://us.sodexo.com/media/news-releases/kiwibot-sodexo.html. Accessed July 
28, 2022. 
23. Starship. Introducing the world’s leading robot delivery service for university college campuses. 
http://starshipdeliveries.com/campus. Accessed July 28, 2022. 
24. Kelso, A. (2022, February 17). Kiwibot and Sodexo to bring over 1K delivery robots to 50 college 
campuses. https://www.restaurantdive.com/news/kiwibot-and-sodexo-to-bring-over-1k-delivery-
robots-to-50-college-campuses/618997. Accessed July 28, 2022. 
25. Zhou, J. Sustainable commute in a car-dominant city: Factors affecting alternative mode choices 
among university students. Transportation Research Part A: Policy and Practice, Vol. 46, No. 7, 2012, 
pp. 1013-1029. 
26. Sanders, R.L., Nelson, T.A. Results from a campus population survey of near misses, crashes, and 
falls while e-scooting, walking, and bicycling. Transportation Research Record, 2022. 
27. Boysen, N., Schwerdfeger, S., Weidinger, F. Scheduling last-mile deliveries with truck-based 
autonomous robots. European Journal of Operational Research, Vol. 271, No. 3, 2018, pp. 1085-
1099. 
28. Simoni, M.D., Kutanoglu, E., Claudel, C.G. Optimization and analysis of a robot-assisted last mile 
delivery system. Transportation Research Part E: Logistics and Transportation Review, Vol. 142, 
2020. 
29. Corno, M., Savaresi, S. Measuring urban sidewalk practicability: A sidewalk robot feasibility index. 
IFAC-PapersOnLine, Vol. 53, No. 2, 2020, pp. 15053-15058. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
49 
 
Data Management Plan 
Products of Research 
Primary data described and analyzed in this report were collected for its first and second studies. For the 
first study, which examined field-recorded videos of pedestrians and bicyclists interacting with sidewalk 
autonomous delivery robots (SADRs) on the main campus of Northern Arizona University (NAU), a data 
set of observed SADR-involved interactions was collected. This raw data set and subsequent versions of 
cleaned tabular data were analyzed to produce those tables and figures in this report’s second chapter. 
For the second study, which analyzed the results from an administered original survey instrument to an 
NAU population, survey respondent data were collected. This second raw primary data set was cleaned, 
summarized, and analyzed by estimating statistical model, with results and other tabular data presented 
in the report’s third chapter. 
 
Data Format and Content 
The tabular data sets collected and analyzed for this research report are formatted as comma separated 
values (.csv) files, with statistical analyses conducted using the open-source R programming language for 
statistical computing and graphics. These .csv data files and .R scripts have been uploaded to a Harvard 
Dataverse (“Replication Data for PSR-21-16”) the contains the following content for the two studies: 
 
• Study 1: Observed Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and 
Bicyclists on Shared-Use Pathways 
o “starship_video_01_data-clean.R” is a script that merges the raw conflict (“conflict-
review_master_v2.tab”), exposure (“exposure-review_master_v2.tab”), and bounding 
box (“bounding-boxes-cdp.tab”) data sets to produce a cleaned study sample data set 
(“dat_pet_clean.tab”). 
o “starship_video_02_models.R” is a script that produces the statistical modeling data set 
(“dat_pet_model.tab”) used to perform the study’s modeling analysis from the cleaned 
study sample data set (“dat_pet_clean.tab”). 
• Study 2: Reported Pedestrian and Bicyclist Comfort in Sharing Pathways with Sidewalk 
Autonomous Robots 
o “starship_survey_01_data-clean.R” is a script that imports and cleans the raw data set 
(“dat_pard_raw.tab”) to produce the study sample data set (“dat_pard_clean.tab”). 
o “starship_survey_03_models_v2.R” is a script that produces the statistical modeling 
data set (“dat_pard_model_v2.tab”) used to perform the study’s modeling analysis from 
the cleaned study sample data set (“dat_pard_clean.tab”). 
 
Data Access and Sharing 
The data sets and analytic scripts used in this research report can be found in “Replication Data for PSR-
21-16” on Harvard Dataverse (https://doi.org/10.7910/DVN/KWMCDM). Video files (.mov) associated 
with the first study will be retained on a password-protected external drive accessible by the Principal 
Investigator, which can be shared with the general public for research purposes upon request. 
 
Reuse and Redistribution 
Tabular data and associated scripts that are published on Dataverse or locally-stored video data may be 
reused and redistributed for research purposes with permission from this report’s Principal Investigator. 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
50 
 
Appendix: Perceptions of Autonomous Robots for Deliveries 
(PAR-D) survey instrument 
 
Section I: Survey Participant Information 
Please answer the following set of questions regarding your sociodemographic and economic 
background and general travel behaviors to the best of your abilities and as accurately as possible. 
1. Which of the following best describes your current living accommodations in relation to NAU? 
• On-campus housing 
• Off-campus housing 
 
2. What ZIP code do you currently live in? 
Note: Display only if “Off-campus housing” is selected for Question 1. 
 
3. What is your age? 
• 18-24 years 
• 25-34 years 
• 35-44 years 
• 45-64 years 
• 65+ years 
 
4. What is your gender? 
• Female 
• Male 
• Self-describe (please specify) 
• Prefer not to answer 
 
5. Which racial/ethnic background do you identify with? Check all that apply. 
• White/Caucasian 
• Latino/Hispanic 
• Black/African American 
• Asian 
• American Indian or Alaska Native 
• Native Hawaiian or other Pacific Islander 
• Self-describe (please specify) 
• Prefer not to answer 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
51 
 
 
6. What was your personal income during the past 12 months? 
• Below $15,000 
• $15,000 - $34,999 
• $35,000 - $49,999 
• $50,000 - $74,999 
• $75,000 - $99,999 
• $100,000 or above 
• Prefer not to answer 
 
7. What is your current employment status? Check all that apply. 
• Full-time work (35 or more hours per week) 
• Part-time work (1-34 hours per week) 
• Full-time student 
• Part-time student 
• Retired 
• Unemployed and looking for work 
• Unemployed and NOT looking for work 
• Other (please specify) 
 
8. Which best describes your educational status? 
Note: Display only if “Full-time student” or “Part-time student” is selected for Question 9. 
• Freshman 
• Sophomore 
• Junior 
• Senior 
• Master’s student 
• Doctoral student 
 
9. Which best describes your educational status? 
Note: Display only if “Full-time student” or “Part-time student” is not selected for Question 9. 
• High school degree or equivalent 
• Associate degree or some college 
• Bachelor’s degree 
• Graduate degree (Masters, PhD) 
 
10. Which ONE of the following ways do you mostly travel TO NAU? 
Note: Display only if “Off-campus housing” is selected for Question 1. 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
52 
 
• Car 
• Bus 
• Bicycle 
• Walk 
• Other (please specify) 
 
11. What are all the ways you travel AROUND NAU? 
• Car 
• Bus 
• Bicycle 
• Walk 
• Other (please specify) 
 
Section II: Autonomous Delivery Vehicles 
Please answer the following set of questions regarding your experiences with and present perceptions 
of sidewalk automated delivery robots to the best of your abilities and as accurately as possible. 
 
12. How often do you use Starship robot delivery services to order food or drinks? 
• Never 
• Very rarely (one time per year or less) 
• Rarely (one time per month or less) 
• Occasionally (two or three times per month) 
• Frequently (one time per week) 
• Very frequently (two or more times per week) 
• Always (one or more times per day) 
 
13. As a PEDESTRIAN or a BICYCLIST, have you altered your intended path because of an interaction with 
an autonomous Starship robot delivery service? 
• Yes 
• No 
• Don’t know 
 
14. As a PEDESTRIAN, what is your comfort in sharing pathways with autonomous robot delivery 
services? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
53 
 
• Comfortable 
• Very Comfortable 
• Don’t know 
 
15. As a BICYCLIST, what is your comfort in sharing pathways with autonomous robot delivery services? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 
• Comfortable 
• Very Comfortable 
• Don’t know or don’t ride a bicycle 
 
16. For the following questions, imagine that in five years the use of autonomous robot delivery services 
is more common in public places. Please answer the following questions based on your opinion and 
judgment, with a score of 1 indicating least agreement and a score of 5 indicating most agreement. 
 
a. I intend to use autonomous delivery vehicles as a food or grocery delivery option. 
• 1 (Least agreement) 
• 2 
• 3 
• 4 
• 5 (Most agreement) 
 
b. Autonomous delivery vehicles will work well if sharing pathways with only pedestrians and 
bicyclists. 
• 1 (Least agreement) 
• 2 
• 3 
• 4 
• 5 (Most agreement) 
 
c. Autonomous delivery vehicles will work well if sharing roadways with only motorists. 
• 1 (Least agreement) 
• 2 
• 3 
• 4 
• 5 (Most agreement) 
 

Evaluation of Sidewalk Autonomous Delivery Robot Interactions with Pedestrians and Bicyclists 
 
54 
 
Section III. Choice Experiment 
Please review the following videos imagining that you are the pedestrian or bicyclist who is 
encountering the autonomous delivery vehicle. Please answer the following set of questions to the best 
of your abilities and as accurately as possible. 
 
17. [Video: Pedestrian and Starship robot with post encroachment time (PET) = 1-3 seconds] 
As a PEDESTRIAN, what is your comfort in sharing this pathway with the autonomous delivery 
vehicle? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 
• Comfortable 
• Very Comfortable 
 
18. [Video: Pedestrian and Starship robot with PET = 0-1 seconds] 
As a PEDESTRIAN, what is your comfort in sharing this pathway with the autonomous delivery 
vehicle? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 
• Comfortable 
• Very Comfortable 
 
19. [Video: Bicyclist and Starship robot with PET = 1-3 seconds] 
As a BICYCLIST, what is your comfort in sharing this pathway with the autonomous delivery vehicle? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 
• Comfortable 
• Very Comfortable 
 
20. [Video: Bicyclist and Starship robot with PET = 0-1 seconds] 
As a BICYCLIST, what is your comfort in sharing this pathway with the autonomous delivery vehicle? 
• Very uncomfortable 
• Uncomfortable 
• Neutral 
• Comfortable 
• Very Comfortable
```
