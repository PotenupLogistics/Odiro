# RSR-004

<!-- page:1 -->
Scenic: A Language for Scenario Specification and
Scene Generation
Daniel J. Fremont
University of California, Berkeley
USA
dfremont@berkeley.edu
Tommaso Dreossi∗
University of California, Berkeley
USA
tommasodreossi@berkeley.edu
Shromona Ghosh∗
University of California, Berkeley
USA
shromona.ghosh@berkeley.edu
Xiangyu Yue∗
University of California, Berkeley
USA
xyyue@berkeley.edu
Alberto L.
Sangiovanni-Vincentelli
University of California, Berkeley
USA
alberto@berkeley.edu
Sanjit A. Seshia
University of California, Berkeley
USA
sseshia@berkeley.edu
Figure 1. Three scenes generated from a single∼20-line Scenic scenario representing bumper-to-bumper traffic.
Abstract
We propose a new probabilistic programming language for
the design and analysis of perception systems, especially
those based on machine learning. Specifically, we consider
the problems of training a perception system to handle rare
events, testing its performance under different conditions,
and debugging failures. We show how a probabilistic pro-
gramming language can help address these problems by
specifying distributions encoding interesting types of inputs
and sampling these to generate specialized training and test
sets. More generally, such languages can be used for cyber-
physical systems and robotics to write environment models,
an essential prerequisite to any formal analysis. In this pa-
∗These authors contributed equally to the paper.
Permission to make digital or hard copies of all or part of this work for
personal or classroom use is granted without fee provided that copies
are not made or distributed for profit or commercial advantage and that
copies bear this notice and the full citation on the first page. Copyrights
for components of this work owned by others than the author(s) must
be honored. Abstracting with credit is permitted. To copy otherwise, or
republish, to post on servers or to redistribute to lists, requires prior specific
permission and/or a fee. Request permissions from permissions@acm.org.
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
© 2019 Copyright held by the owner/author(s). Publication rights licensed
to ACM.
ACM ISBN 978-1-4503-6712-7/19/06. . . $15.00
https://doi.org/10.1145/3314221.3314633
per, we focus on systems like autonomous cars and robots,
whose environment is a scene, a configuration of physical
objects and agents. We design a domain-specific language,
Scenic, for describing scenarios that are distributions over
scenes. As a probabilistic programming language, Scenic
allows assigning distributions to features of the scene, as
well as declaratively imposing hard and soft constraints over
the scene. We develop specialized techniques for sampling
from the resulting distribution, taking advantage of the struc-
ture provided by Scenic’s domain-specific syntax. Finally,
we apply Scenic in a case study on a convolutional neural
network designed to detect cars in road images, improving
its performance beyond that achieved by state-of-the-art
synthetic data generation methods.
CCS Concepts • Software and its engineering → Do-
main specific languages ; Software testing and debug-
ging; Specification languages ; • Computing methodolo-
gies → Machine learning; Computer vision.
Keywords scenario description language, synthetic data,
deep learning, probabilistic programming, automatic test
generation, fuzz testing
ACM Reference Format:
Daniel J. Fremont, Tommaso Dreossi, Shromona Ghosh, Xiangyu
Yue, Alberto L. Sangiovanni-Vincentelli, and Sanjit A. Seshia. 2019.
Scenic: A Language for Scenario Specification and Scene Gen-
eration. In Proceedings of the 40th ACM SIGPLAN Conference on
arXiv:1809.09310v2  [cs.PL]  21 Jun 2019

<!-- page:2 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Programming Language Design and Implementation (PLDI ’19), June
22–26, 2019, Phoenix, AZ, USA. ACM, New York, NY, USA, 41 pages.
https://doi.org/10.1145/3314221.3314633
1 Introduction
Machine learning (ML) is increasingly used in safety-critical
applications, thereby creating an acute need for techniques
to gain higher assurance in ML-based systems [1, 40, 42]. ML
has proved particularly effective at perceptual tasks such as
speech and vision. Thus, there is a pressing need to tackle
several important problems in the design of such ML-based
perception systems, including:
• training the system so that it correctly responds to events
that happen only rarely,
• testing the system under a variety of conditions, especially
unusual ones, and
• debugging the system to understand the root cause of a
failure and eliminate it.
The traditional ML approach to these problems is to gather
more data from the environment, retraining the system until
its performance is adequate. The major difficulty here is that
collecting real-world data can be slow and expensive, since
it must be preprocessed and correctly labeled before use.
Furthermore, it may be difficult or impossible to collect data
for corner cases that are rare but nonetheless necessary to
train and test against: for example, a car accident. As a result,
recent work has investigated training and testing systems
with synthetically generated data, which can be produced in
bulk with correct labels and giving the designer full control
over the distribution of the data [22, 23, 25, 45].
A challenge to the use of synthetic data is that it can be
highly non-trivial to generate meaningful data, since this
usually requires modeling complex environments [42]. Sup-
pose we wanted to train a network on images of cars on a
road. If we simply sampled uniformly at random from all
possible configurations of, say, 12 cars, we would get data
that was at best unrealistic, with cars facing sideways or
backward, and at worst physically impossible, with cars in-
tersecting each other. Instead, we want scenes like those in
Fig. 1, where the cars are laid out in a consistent and real-
istic way. Furthermore, we may want scenes that are not
only realistic but represent particular scenarios of interest
for training or testing, e.g., parked cars, cars passing across
the field of view, or bumper-to-bumper traffic as in Fig. 1.
In general, we need a way to guide data generation toward
scenes that make sense for our application.
We argue that probabilistic programming languages (PPLs)
provide a natural solution to this problem. Using a PPL, the
designer of a system can construct distributions representing
different input regimes of interest, and sample from these
distributions to obtain concrete inputs for training and test-
ing. More generally, the designer can model the system’s
environment, with the program becoming a specification of
the distribution of environments under which the system
is expected to operate correctly with high probability. Such
environment models are essential for any formal analysis: in
particular, composing the system with the model, we obtain a
closed program which we could potentially prove properties
about to establish the correctness of the system.
In this paper, we focus on designing and analyzing sys-
tems whose environment is a scene, a configuration of ob-
jects in space (including dynamic agents, such as vehicles).
We develop a domain-specific scenario description language,
Scenic, to specify such environments. Scenic is a proba-
bilistic programming language, and a Scenic scenario de-
fines a distribution over scenes. As we will see, the syntax
of the language is designed to simplify the task of writing
complex scenarios, and to enable the use of specialized sam-
pling techniques. In particular, Scenic allows the user to
both construct objects in a straightforward imperative style
and impose hard and soft constraints declaratively. It also
provides readable, concise syntax for common geometric re-
lationships that would otherwise require complex non-linear
expressions and constraints. In addition, Scenic provides a
notion of classes allowing properties of objects to be given
default values depending on other properties: for example,
we can define a Car so that by default it faces in the direc-
tion of the road at its position. More broadly, Scenic uses
a novel approach to object construction which factors the
process into syntactically-independent specifiers which can
be combined in arbitrary ways, mirroring the flexibility of
natural language. Finally, Scenic provides an easy way to
generalize a concrete scene by automatically adding noise.
Generating scenes from a Scenic scenario requires sam-
pling from the probability distribution it implicitly defines.
This task is closely related to the inference problem for im-
perative PPLs with observations [21]. While Scenic could
be implemented as a library on top of such a language, we
found that clarity and concision could be significantly im-
proved with new syntax (specifiers in particular) difficult to
implement as a library. Furthermore, while Scenic could be
translated into existing PPLs, using a new language allows
us to impose restrictions enabling domain-specific sampling
techniques not possible with general-purpose PPLs. In par-
ticular, we develop algorithms which take advantage of the
particular structure of distributions arising from Scenic pro-
grams to dramatically prune the sample space.
Finally, we demonstrate the utility of Scenic in training,
testing, and debugging perception systems with a case study
on SqueezeDet [49], a convolutional neural network for ob-
ject detection in autonomous cars. For this task, it has been
shown [25] that good performance on real images can be
achieved with networks trained purely on synthetic images
from the video game Grand Theft Auto V (GTAV [15]). We
implemented a sampler for Scenic scenarios, using it to gen-
erate scenes which were rendered into images by GTAV. Our
experiments demonstrate using Scenic to:

<!-- page:3 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
• evaluate the accuracy of the ML system under particular
conditions, e.g. in good or bad weather,
• improve performance in corner cases by emphasizing
them during training: we use Scenic to both identify a
deficiency in a state-of-the-art car detection data set [25]
and generate a new training set of equal size but yielding
significantly better performance, and
• debug a known failure case by generalizing it in many
directions, exploring sensitivity to different features and
developing a more general scenario for retraining: we
use Scenic to find an image the network misclassifies,
discover the root cause, and fix the bug, in the process
improving the network’s performance on its original test
set (again, without increasing training set size).
These experiments show that Scenic can be a very useful
tool for understanding and improving perception systems.
While our main case study is performed in the domain of
visual perception for autonomous driving, and uses one par-
ticular simulator (GTAV), we stress thatScenic is not specific
to either. In Sec. 3 we give an example of a different domain
(robotic motion planning) and simulator (Webots [31]), and
we are currently also using Scenic with the CARLA driv-
ing simulator [6] and the X-Plane flight simulator [36] (see
Sec. 8). Generally,Scenic can produce data of any desired type
(e.g. RGB images, LIDAR point clouds, or trajectories from
dynamical simulations) by interfacing it to an appropriate
simulator. This requires only two steps: (1) writing a small
Scenic library defining the types of objects supported by
the simulator, as well as the geometry of the workspace; (2)
writing an interface layer converting the configurations out-
put by Scenic into the simulator’s input format. While the
current version of Scenic is primarily concerned with geom-
etry, leaving the details of rendering up to the simulator, the
language allows putting distributions on any parameters the
simulator exposes: for example, in GTAV the meshes of the
various car models are fixed but we can control their overall
color. We have also usedScenic to specify distributions over
parameters on system dynamics.
In summary, the main contributions of this work are:
• Scenic, a domain-specific probabilistic programming
language for describing scenarios: distributions over
configurations of physical objects and agents;
• a methodology for using PPLs to design and analyze
perception systems, especially those based on ML;
• domain-specific algorithms for sampling from the dis-
tribution defined by a Scenic program;
• a case study using Scenic to analyze and improve
the accuracy of a practical deep neural network for
autonomous driving beyond what is achieved by state-
of-the-art synthetic data generation methods.
The paper is structured as follows: we begin with an
overview of our approach in Sec. 2. Section 3 gives examples
highlighting the major features of Scenic and motivating
various choices in its design. In Sec. 4 we describe theScenic
language in detail, and in Sec. 5 we discuss its formal seman-
tics and our sampling algorithms. Section 6 describes the
experimental setup and results of our car detection case
study. Finally, we discuss related work in Sec. 7 and conclude
in Sec. 8 with a summary and directions for future work.
An early version of this paper appeared as [12]. For the
Appendices and our implementation code, see [14].
2 Using PPLs to Design and Analyze
Perception Systems
Scenic
Sampler
Scenic
Program
Training
Data
Test
Data
SimulatorScenes System
Failure
Cases
Figure 2. Tool flow using Scenic to train, test, and debug a
perception system.
We propose a methodology for training, testing, and de-
bugging perception systems using probabilistic program-
ming languages. The core idea is to use PPLs to formalize
general operation scenarios, then sample from these distri-
butions to generate concrete environment configurations.
Putting these configurations into a simulator, we obtain im-
ages or other sensor data which can be used to test and train
the perception system. The general procedure is outlined
in Fig. 2. Note that the training/testing datasets need not
be purely synthetic: we can generate data to supplement
existing real-world data (possibly mitigating a deficiency in
the latter, while avoiding overfitting). Furthermore, even for
models trained purely on real data, synthetic data can still be
useful for testing and debugging, as we will see below. Now
we discuss the three design problems from the Introduction
in more detail.
Testing under Different Conditions. The most straight-
forward problem is that of assessing system performance
under different conditions. We can simply write scenarios
capturing each condition, generate a test set from each one,
and evaluate the performance of the system on these. Note
that conditions which occur rarely in the real world present
no additional problems: as long as the PPL we use can encode
the condition, we can generate as many instances as desired.
Training on Rare Events. Extending the previous applica-
tion, we can use this procedure to help ensure the system
performs adequately even in unusual circumstances or par-
ticularly difficult cases. Writing a scenario capturing these
rare events, we can generate instances of them to augment or
replace part of the original training set. Emphasizing these

<!-- page:4 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
instances in the training set can improve the system’s per-
formance in the hard case without impacting performance
in the typical case. In Sec. 6.3 we will demonstrate this for
car detection, where a hard case is when one car partially
overlaps another in the image. We wrote a Scenic program
to generate a set of these overlapping images. Training the
car-detection network on a state-of-the-art synthetic dataset
obtained by randomly driving around inside the simulated
world of GTAV and capturing images periodically [25], we
find its performance is significantly worse on the overlapping
images. However, if we keep the training set size fixed but
increase the proportion of overlapping images, performance
on such images dramatically improves without harming per-
formance on the original generic dataset .
Debugging Failures. Finally, we can use the same proce-
dure to help understand and fix bugs in the system. If we find
an environment configuration where the system fails, we can
write a scenario reproducing that particular configuration.
Having the configuration encoded as a program then makes
it possible to explore the neighborhood around it in a variety
of different directions, leaving some aspects of the scene
fixed while varying others. This can give insight into which
features of the scene are relevant to the failure, and eventu-
ally identify the root cause. The root cause can then itself
be encoded into a scenario which generalizes the original
failure, allowing retraining without overfitting to the partic-
ular counterexample. We will demonstrate this approach in
Sec. 6.4, starting from a single misclassification, identifying
a general deficiency in the training set, replacing part of the
training data to fix the gap, and ultimately achieving higher
performance on the original test set.
For all of these applications we need a PPL which can
encode a wide range of general and specific environment
scenarios. In the next section, we describe the design of a
language suited to this purpose.
3 The Scenic Language
We use Scenic scenarios from our autonomous car case
study to motivate and illustrate the main features of the
language, focusing on features that makeScenic particularly
well-suited for the domain of generating data for perception
systems.
Basics: Classes, Objects, Geometry, and Distributions.
To start, suppose we want scenes of one car viewed from
another on the road. We can simply write:
1 import gtaLib
2 ego = Car
3 Car
First, we import a library gtaLib containing everything spe-
cific to our case study: the class Car and information about
the locations of roads (from now on we suppress this line).
Only general geometric concepts are built into Scenic.
The second line creates a Car and assigns it to the special
variable ego specifying the ego object which is the reference
point for the scenario. In particular, rendered images from
the scenario are from the perspective of the ego object (it is
a syntax error to leave ego undefined). Finally, the third line
creates an additional Car. Note that we have not specified
the position or any other properties of the two cars: this
means they are inherited from the default values defined in
the class Car. Object-orientation is valuable in Scenic since
it provides a natural organizational principle for scenarios
involving different types of physical objects. It also improves
compositionality, since we can define a generic Car model
in a library like gtaLib and use it in different scenarios. Our
definition of Car begins as follows (slightly simplified):
1 class Car:
2 position: Point on road
3 heading: roadDirection at self.position
Here road is a region (one of Scenic’s primitive types) de-
fined in gtaLib to specify which points in the workspace are
on a road. Similarly, roadDirection is a vector field spec-
ifying the prevailing traffic direction at such points. The
operator F at X simply gets the direction of the field F at
point X, so the default value for a car’s heading is the road
direction at its position. The default position, in turn, is a
Point on road (we will explain this syntax shortly), which
means a uniformly random point on the road.
The ability to make random choices like this is a key aspect
of Scenic. Scenic’s probabilistic nature allows it to model
real-world stochasticity, for example encoding a distribution
for the distance between two cars learned from data. This
in turn is essential for our application of PPLs to training
perception systems: using randomness, a PPL can generate
training data matching the distribution the system will be
used under. Scenic provides several basic distributions (and
allows more to be defined). For example, we can write
1 Car offset by (-10, 10) @ (20, 40)
to create a car that is 20–40 m ahead of the camera. The
interval notation (X , Y ) creates a uniform distribution on
the interval, and X @ Y creates a vector fromxy coordinates
(as in Smalltalk [16]).
Local Coordinate Systems. Using offset by as above over-
rides the default position of the Car, leaving the default ori-
entation (along the road) unchanged. Suppose for greater
realism we don’t want to require the car to beexactly aligned
with the road, but to be within say 5◦. We could try:
1 Car offset by (-10, 10) @ (20, 40),
2 facing (-5, 5) deg
but this is not quite what we want, since this sets the orienta-
tion of the Car in global coordinates (i.e. within 5◦ of North).
Instead we can use Scenic’s general operator X relative
to Y, which can interpret vectors and headings as being in
a variety of local coordinate systems:

<!-- page:5 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
1 Car offset by (-10, 10) @ (20, 40),
2 facing (-5, 5) deg relative to roadDirection
If we want the heading to be relative to the ego car’s orien-
tation, we simply write (-5, 5) deg relative to ego .
Notice that since roadDirection is a vector field, it de-
fines a coordinate system at each point, and an expression
like 15 deg relative to field does not define a unique
heading. The example above works because Scenic knows
that (-5, 5) deg relative to roadDirection depends
on a reference position, and automatically uses theposition
of the Car being defined. This is a feature ofScenic’s system
of specifiers, which we explain next.
Readable, Flexible Specifiers. The syntax offset by X
and facing Y for specifying positions and orientations may
seem unusual compared to typical constructors in object-
oriented languages. There are two reasons why Scenic uses
this kind of syntax: first, readability. The second is more
subtle and based on the fact that in natural language there are
many ways to specify positions and other properties, some
of which interact with each other. Consider the following
ways one might describe the location of an object:
1. “is at position X” (absolute position);
2. “is just left of position X” (pos. based on orientation);
3. “is 3 m left of the taxi” (a local coordinate system);
4. “is one lane left of the taxi” (another local system);
5. “appears to be 10 m behind the taxi” (relative to the line
of sight);
These are all fundamentally different from each other: e.g.,
(3) and (4) differ if the taxi is not parallel to the lane.
Furthermore, these specifications combine other proper-
ties of the object in different ways: to place the object “just
left of” a position, we must first know the object’s heading;
whereas if we wanted to face the object “towards” a location,
we must instead know its position. There can be chains of
such dependencies: “the car is 0.5 m left of the curb” means
that the right edge of the car is 0.5 m away from the curb, not
the car’s position, which is its center. So the car’sposition
depends on its width, which in turn depends on its model.
In a typical object-oriented language, this might be handled
by computing values for position and other properties and
passing them to a constructor. For “a car is 0.5 m left of the
curb” we might write:
1 m = Car.defaultModelDistribution.sample()
2 pos = curb.offsetLeft(0.5 + m.width / 2)
3 car = Car(pos, model=m)
Notice how m must be used twice, because m determines
both the model of the car and (indirectly) its position. This
is inelegant and breaks encapsulation because the default
model distribution is used outside of the Car constructor.
The latter problem could be fixed by having a specialized
constructor or factory function,
1 car = CarLeftOfBy(curb, 0.5)
but these would proliferate since we would need to handle
all possible combinations of ways to specify different prop-
erties (e.g. do we want to require a specific model? Are we
overriding the width provided by the model for this spe-
cific car?). Instead of having a multitude of such monolithic
constructors, Scenic factors the definition of objects into
potentially-interacting but syntactically-independent parts:
1 Car left of spot by 0.5, with model BUS
Here left of X by D and with model M are specifiers
which do not have an order, but which together specify the
properties of the car. Scenic works out the dependencies
between properties (here, position is provided by left of ,
which depends on width, whose default value depends on
model) and evaluates them in the correct order. To use the
default model distribution we would simply leave off with
model BUS ; keeping it affects the position appropriately
without having to specify BUS more than once.
Specifying Multiple Properties Together. Recall that we
defined the default position for a Car to be a Point on
road: this is an example of another specifier, on region,
which specifies position to be a uniformly random point in
the given region. This specifier illustrates another feature of
Scenic, namely that specifiers can specify multiple proper-
ties simultaneously. Consider the following scenario, which
creates a parked car given a region curb defined in gtaLib:
1 spot = OrientedPoint on visible curb
2 Car left of spot by 0.25
The function visible region returns the part of the region
that is visible from the ego object. The specifier on visible
curb will then set position to be a uniformly random visi-
ble point on the curb. We create spot as an OrientedPoint,
which is a built-in class that defines a local coordinate system
by having both a position and a heading. The on region
specifier can also specify heading if the region has a pre-
ferred orientation (a vector field) associated with it: in our
example, curb is oriented by roadDirection. So spot is, in
fact, a uniformly random visible point on the curb, oriented
along the road. That orientation then causes the car to be
placed 0.25 m left of spot in spot’s local coordinate system,
i.e. away from the curb, as desired.
In fact, Scenic makes it easy to elaborate the scenario
without needing to alter the code above. Most simply, we
could specify a particular model or non-default distribution
over models by just adding with model M to the definition
of the Car. More interestingly, we could produce a scenario
for badly-parked cars by adding two lines:
1 spot = OrientedPoint on visible curb
2 badAngle = Uniform(1.0, -1.0) * (10, 20) deg
3 Car left of spot by 0.5,
4 facing badAngle relative to roadDirection

<!-- page:6 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Figure 3. A scene of a badly-parked car.
This will yield cars parked 10-20◦ off from the direction of
the curb, as seen in Fig. 3. This illustrates how specifiers
greatly enhance Scenic’s flexibility and modularity.
Declarative Specifications of Hard and Soft Constraints.
Notice that in the scenarios above we never explicitly en-
sured that the two cars will not intersect each other. Despite
this, Scenic will never generate such scenes. This is because
Scenic enforces several default requirements: all objects must
be contained in the workspace, must not intersect each other,
and must be visible from the ego object.1 Scenic also allows
the user to define custom requirements checking arbitrary
conditions built from various geometric predicates. For ex-
ample, the following scenario produces a car headed roughly
towards us, while still facing the nominal road direction:
1 car2 = Car offset by (-10, 10) @ (20, 40),
2 with viewAngle 30 deg
3 require car2 can see ego
Here we have used the X can see Y predicate, which in
this case is checking that the ego car is inside the 30◦ view
cone of the second car. If we only need this constraint to hold
part of the time, we can use asoft requirement specifying the
minimum probability with which it must hold:
1 require[0.5] car2 can see ego
Hard requirements, called “observations” in other PPLs (see,
e.g., [21]), are very convenient in our setting because they
make it easy to restrict attention to particular cases of inter-
est. They also improve encapsulation, since we can restrict
an existing scenario without altering it. Finally, soft require-
ments are useful in ensuring adequate representation of a
particular condition when generating a training set: for ex-
ample, we could require that at least 90% of the images have
a car driving on the right side of the road.
Mutations. Scenic provides a simple mutation system that
improves compositionality by providing a mechanism to
1The last requirement ensures that the object will affect the rendered image.
It can be disabled, if for example generating non-visual data.
Figure 4. Webots scene of Mars rover in debris field.
add variety to a scenario without changing its code. This is
useful, for example, if we have a scenario encoding a single
concrete scene obtained from real-world data and want to
quickly generate variations. For instance:
1 taxi = Car at 120 @ 300, facing 37 deg, ...
2 ...
3 mutate taxi
This will add Gaussian noise to the position and heading
of taxi, while still enforcing all built-in and custom require-
ments. The standard deviation of the noise can be scaled by
writing, for example, mutate taxi by 2 (which adds twice
as much noise), and we will see later that it can be controlled
separately for position and heading.
Multiple Domains and Simulators. We conclude this sec-
tion with an example illustrating a second application do-
main, namely generating workspaces to test motion planning
algorithms, and Scenic’s ability to work with different sim-
ulators. A robot like a Mars rover able to climb over rocks
can have very complex dynamics, with the feasibility of a
motion plan depending on exact details of the robot’s hard-
ware and the geometry of the terrain. We can use Scenic to
write a scenario generating challenging cases for a planner
to solve. Figure 4 shows a scene, visualized using an interface
we wrote between Scenic and the Webots robotics simula-
tor [31], with a bottleneck between the robot and its goal
that forces the planner to consider climbing over a rock.
This example, the badly-parked car scenario of Fig. 3, and
the bumper-to-bumper traffic scenario of Fig. 1 illustrate the
versatility of Scenic in constructing a wide range of inter-
esting scenarios. Complete Scenic code for the bumper-to-
bumper scenario as well as other scenarios used as examples
in this section or in our experiments, along with images of
generated scenes, can be found in Appendix A.

<!-- page:7 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
scenario := (import file)∗ (statement)∗
boolean := True | False | booleanOperator
scalar := number | distrib | scalarOperator
distrib := baseDist | resample(distrib)
vector := scalar @ scalar | Point | vectorOperator
heading := scalar | OrientedPoint | headingOperator
direction := heading | vectorField
value := boolean | scalar | vector | direction
| region | instance | instance.property
classDefn := class class[(superclass)]:
(property: defaultValueExpr)∗
instance := class specifier , . . .
specifier := with property value | posSpec | headSpec
Figure 5. Simplified Scenic grammar. Point and Oriented-
Point are instances of the corresponding classes. See Tab. 5
for statements, Fig. 7 for operators, Tab. 1 for baseDist, and
Tables 3 and 4 for posSpec and headSpec.
4 Syntax of Scenic
Scenic is a simple object-oriented PPL, with programs con-
sisting of sequences of statements built with standard im-
perative constructs including conditionals, loops, functions,
and methods (which we do not describe further, focusing on
the new elements). Compared to other imperative PPLs, the
major restriction of Scenic, made in order to allow more
efficient sampling, is that conditional branching may not de-
pend on random variables. The novel syntax, outlined above,
is largely devoted to expressing geometric relationships in a
concise and flexible manner. Figure 5 gives a formal grammar
for Scenic, which we now describe in detail.
4.1 Data Types
Scenic provides several primitive data types:
Booleans expressing truth values.
Scalars floating-point numbers, which can be sampled
from various distributions (see Table 1).
Vectors representing positions and offsets in space, con-
structed from coordinates in meters with the syntax
X @ Y (inspired by Smalltalk [16]).
Headings representing orientations in space. Conve-
niently, in 2D these are a single angle (in radians, anti-
clockwise from North). By convention the heading of a
local coordinate system is the heading of itsy-axis, so,
for example, -2 @ 3 means 2 meters left and 3 ahead.
Vector Fields associating an orientation to each point in
space. For example, the shortest paths to a destination
or (in our case study) the nominal traffic direction.
Regions representing sets of points in space. These can
have an associated vector field giving points in the
region preferred orientations.
Table 1. Distributions. All parameters scalars except value.
Syntax Distribution
(low, high) uniform on interval
Uniform(value, . . .) uniform over values
Discrete({value: wt, . . .}) discrete with weights
Normal(mean, stdDev) normal with givenµ,σ
Table 2. Properties of Point, OrientedPoint, and Object.
Property Default Meaning
position 0 @ 0 position in global coords.
viewDistance 50 distance for ‘can see ’
mutationScale 0 overall scale of mutations
positionStdDev 1 mutationσ for position
heading 0 heading in global coords.
viewAngle 360◦ angle for ‘can see ’
headingStdDev 5◦ mutationσ for heading
width 1 width of bounding box
height 1 height of bounding box
allowCollisions false collisions allowed
requireVisible true must be visible from ego
In addition, Scenic provides objects, organized into single-
inheritance classes specifying a set of properties their in-
stances must have, together with corresponding default val-
ues (see Fig. 5). Default value expressions are evaluated each
time an object is created. Thus if we write weight: (1, 5)
when defining a class then each instance will have a weight
drawn independently from (1, 5) . Default values may use
the special syntax self.property to refer to one of the other
properties of the object, which is then a dependency of this
default value. In our case study, for example, the width and
height of a Car are by default derived from its model.
Physical objects in a scene are instances of Object, which
is the default superclass when none is specified. Object de-
scends from the two other built-in classes: its superclass
is OrientedPoint, which in turn subclasses Point. These
represent locations in space, with and without an orienta-
tion respectively, and so provide the fundamental properties
heading and position. Object extends them by defining a
bounding box with the propertieswidth and height. Table 2
lists the properties of these classes and their default values.
To allow cleaner notation, Point and OrientedPoint are
automatically interpreted as vectors or headings in contexts
expecting these (as shown in Fig. 5). For example, we can
write taxi offset by 1 @ 2 and 30 deg relative to
taxi instead of taxi.position offset by 1 @ 2 and 30
deg relative to taxi.heading . Ambiguous cases, e.g.
taxi relative to limo , are illegal (caught by a simple
type system); the more verbose syntax must be used instead.

<!-- page:8 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
ego
left of ego
back right of ego
1
2 Point offset by 1 @ 2
or
1 @ 2 relative to ego
P
P offset by 0 @ -2
2
21
Point beyond P by -2 @ 1
Object behind P by 2
apparent heading of P
Figure 6. Various Scenic operators and specifiers applied
to the ego object and an OrientedPoint P . Instances of
OrientedPoint are shown as bold arrows.
4.2 Expressions
Scenic’s expressions are mostly straightforward, largely con-
sisting of the arithmetic, boolean, and geometric operators
shown in Fig. 7. The meanings of these operators are largely
clear from their syntax, so we defer complete definitions of
their semantics to Appendix C. Figure 6 illustrates several of
the geometric operators (as well as some specifiers, which
we will discuss in the next section). Various points to note:
• X can see Y uses a simple model where aPoint can see
a certain distance, and an OrientedPoint restricts this
to the sector along its heading with a certain angle (see
Table 2). An Object is visible iff its bounding box is.
• X relative to Y interprets X as an offset in a local
coordinate system defined by Y. Thus -3 @ 0 relative
to Y yields 3 m West of Y if Y is a vector, and 3 m left of
Y if Y is an OrientedPoint. If defining a heading inside a
specifier, either X or Y can be a vector field, interpreted as
a heading by evaluating it at the position of the object
being specified. So we can write for example Car at 120
@ 70, facing 30 deg relative to roadDirection .
• visible region yields the part of the region visible from
the ego, e.g. Car on visible road . The form region
visible from X uses X instead of ego.
Two types of Scenic expressions are more complex: dis-
tributions and object definitions. As in a typical imperative
probabilistic programming language, a distribution evaluates
to a sample from the distribution. Thus the program
1 x = (0, 1)
2 y = x @ x
does not makey uniform over the unit box, but rather over its
diagonal. For convenience in sampling multiple times from
a primitive distribution, Scenic provides a resample(D)
scalarOperator := max(scalar, . . .) | min(scalar, . . .)
| -scalar | abs(scalar) | scalar (+ | *) scalar
| relative heading of heading [from heading]
| apparent heading of OrientedPoint [from vector]
| distance [from vector] to vector
| angle [from vector] to vector
booleanOperator := not boolean
| boolean (and | or) boolean
| scalar (== | != | < | > | <= | >=) scalar
| (Point | OrientedPoint) can see (vector | Object)
| (vector | Object) is in region
headingOperator := scalar deg
| vectorField at vector
| direction relative to direction
vectorOperator := vector relative to vector
| vector offset by vector
| vector offset along direction by vector
regionOperator := visible region
| region visible from (Point | OrientedPoint)
orientedPointOperator :=
vector relative to OrientedPoint
| OrientedPoint offset by vector
| follow vectorField [from vector] for scalar
| (front | back | left | right) of Object
| (front | back) ( left | right) of Object
Figure 7. Operators by result type.
function returning an independent 2 sample from D, one
of the distributions in Tab. 1. Scenic also allows defining
custom distributions beyond those in the Table.
The second type of complexScenic expressions are object
definitions. These are the only expressions with a side ef-
fect, namely creating an object in the generated scene. More
interestingly, properties of objects are specified using the
system of specifiers discussed above, which we now detail.
4.3 Specifiers
As shown in the grammar in Fig. 5, an object is created
by writing the class name followed by a (possibly empty)
comma-separated list of specifiers. The specifiers are com-
bined, possibly adding default specifiers from the class defi-
nition, to form a complete specification of all properties of
the object. Arbitrary properties (including user-defined prop-
erties with no meaning in Scenic) can be specified with the
generic specifier with property value , whileScenic provides
many more specifiers for the built-in properties position
and heading, shown in Tables 3 and 4 respectively.
In general, a specifier is a function taking in values for zero
or more properties, its dependencies, and returning values for
one or more other properties, some of which can be specified
2Conditioned on the values of the distribution’s parameters (e.g. low and
high for a uniform interval), which are not resampled.

<!-- page:9 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
Table 3. Specifiers for position. Those in the second group also optionally specify heading.
Specifier Dependencies
at vector —
offset by vector —
offset along direction by vector —
(left | right) of vector [by scalar] heading, width
(ahead of | behind) vector [by scalar] heading, height
beyond vector by vector [from vector] —
visible [from (Point | OrientedPoint)] —
(in | on) region —
(left | right) of (OrientedPoint | Object) [ by scalar] width
(ahead of | behind) ( OrientedPoint | Object) [ by scalar] height
following vectorField [from vector] for scalar —
Table 4. Specifiers for heading.
Specifier Deps.
facing heading —
facing vectorField position
facing (toward | away from ) vector position
apparently facing heading [from vector] position
optionally, meaning that other specifiers will override them.
For example, on region specifies position and optionally
specifies heading if the given region has a preferred orien-
tation. If road is such a region, as in our case study, then
Object on road will create an object at a position uniformly
random in road and with the preferred orientation there. But
since heading is only specified optionally, we can override
it by writing Object on road, facing 20 deg .
Specifiers are combined to determine the properties of an
object by evaluating them in an order ensuring that their
dependencies are always already assigned. If there is no such
order or a single property is specified twice, the scenario
is ill-formed. The procedure by which the order is found,
taking into account properties that are optionally specified
and default values, will be described in the next section.
As the semantics of the specifiers in Tables 3 and 4 are
largely evident from their syntax, we defer exact definitions
to Appendix C. We briefly discuss some of the more complex
specifiers, referring to the examples in Fig. 6:
• behind vector means the object is placed with the mid-
point of its front edge at the given vector, and similarly
for ahead/left/right of vector.
• beyond A by O from B means the position obtained
by treating O as an offset in the local coordinate system
at A oriented along the line of sight from B. In this and
other specifiers, if the from B is omitted, the ego object
is used by default. So for example beyond taxi by 0
Table 5. Statements.
Syntax Meaning
identifier = value var. assignment
param identifier = value, . . . param. assign.
classDefn class definition
instance object definition
require boolean hard requirement
require[number] boolean soft requirement
mutate identifier, . . . [ by number] enable mutation
@ 3 means 3 m directly behind the taxi as viewed by the
camera (see Fig. 6 for another example).
• The heading optionally specified by left of Oriented-
Point, etc. is that of the OrientedPoint (thus in Fig. 6, P
offset by 0 @ -2 yields an OrientedPoint facing the
same way as P). Similarly, the heading optionally speci-
fied by following vectorField is that of the vector field
at the specified position.
• apparently facing H means the object has heading H
with respect to the line of sight from ego. For example,
apparently facing 90 deg would orient the object so
that the camera views its left side head-on.
4.4 Statements
Finally, we discuss Scenic’s statements, listed in Table 5.
Class and object definitions have been discussed above, and
variable assignment behaves in the standard way.
The statement param identifer = value assigns values to
global parameters of the scenario. These have no semantics
in Scenic but provide a general-purpose way to encode arbi-
trary global information. For example, in our case study we
used parameters time and weather to put distributions on
the time of day and the weather conditions during the scene.

<!-- page:10 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
The require boolean statement requires that the given
condition hold in all instantiations of the scenario (equiva-
lently to observe statements in other probabilistic program-
ming languages; see e.g. [ 5, 32]). The variant statement
require[p] boolean adds a soft requirement that need only
hold with some probabilityp (which must be a constant). We
will discuss the semantics of these in the next section.
Lastly, the mutate instance, . . . by number statement
adds Gaussian noise with the given standard deviation (de-
fault 1) to the position and heading properties of the listed
objects (or every Object, if no list is given). For example,
mutate taxi by 2 would add twice as much noise as
mutate taxi . The noise can be controlled separately for
position and heading, as we discuss in the next section.
5 Semantics and Scene Generation
5.1 Semantics of Scenic
The output of a Scenic program is a scene consisting of the
assignment to all the properties of each Object defined in
the scenario, plus any global parameters defined with param.
Since Scenic is a probabilistic programming language, the
semantics of a program is actually a distribution over possi-
ble outputs, here scenes. As for other imperative PPLs, the
semantics can be defined operationally as a typical inter-
preter for an imperative language but with two differences.
First, the interpreter makes random choices when evaluat-
ing distributions [41]. For example, the Scenic statement
x = (0, 1) updates the state of the interpreter by assigning
a value to x drawn from the uniform distribution on the in-
terval(0, 1). In this way every possible run of the interpreter
has a probability associated with it. Second, every run where
a require statement (the equivalent of an “observation” in
other PPLs) is violated gets discarded, and the run probabili-
ties appropriately normalized (see, e.g., [21]). For example,
adding the statement require x > 0.5 above would yield
a uniform distribution for x over the interval(0.5, 1).
Scenic uses the standard semantics for assignments, arith-
metic, loops, functions, and so forth. Below, we define the
semantics of the main constructs unique to Scenic. See Ap-
pendix B for a more formal treatment.
Soft Requirements. The statement require[p] B is inter-
preted as require B with probability p and as a no-op oth-
erwise: that is, it is interpreted as a hard requirement that is
only checked with probability p. This ensures that the con-
dition B will hold with probability at least p in the induced
distribution of the Scenic program, as desired.
Specifiers and Object Definitions. As we saw above, each
specifier defines a function mapping values for its dependen-
cies to values for the properties it specifies. When an object
of class C is constructed using a set of specifiersS, the object
is defined as follows (see Appendix B for details):
1. If a property is specified (non-optionally) by multiple
specifiers in S, an ambiguity error is raised.
2. The set of properties P for the new object is found by
combining the properties specified by all specifiers in S
with the properties inherited from the class C.
3. Default value specifiers from C are added to S as needed
so that each property inP is paired with a unique specifier
in S specifying it, with precedence order: non-optional
specifier, optional specifier, then default value.
4. The dependency graph of the specifiers S is constructed.
If it is cyclic, an error is raised.
5. The graph is topologically sorted and the specifiers are
evaluated in this order to determine the values of all prop-
erties P of the new object.
Mutation. The mutate X by N statement sets the special
mutationScale property to N (the mutate X form sets it to
1). At the end of evaluation of theScenic program, but before
requirements are checked, Gaussian noise is added to the
position and heading properties of objects with nonzero
mutationScale. The standard deviation of the noise is the
value of the positionStdDev and headingStdDev property
respectively (see Table 2), multiplied by mutationScale.
The problem of sampling scenes from the distribution de-
fined by a Scenic program is essentially a special case of
the sampling problem for imperative PPLs with observations
(since soft requirements can also be encoded as observations).
While we could apply general techniques for such problems,
the domain-specific design of Scenic enables specialized
sampling methods, which we discuss below. We also note
that the scene generation problem is closely related tocontrol
improvisation, an abstract framework capturing various prob-
lems requiring synthesis under hard, soft, and randomness
constraints [13]. Scene improvisation from a Scenic program
can be viewed as an extension with a more detailed random-
ness constraint given by the imperative part of the program.
5.2 Domain-Specific Sampling Techniques
The geometric nature of the constraints in Scenic programs,
together with Scenic’s lack of conditional control flow, en-
able domain-specific sampling techniques inspired by robotic
path planning methods. Specifically, we can use ideas for
constructing configuration spaces to prune parts of the sam-
ple space where the objects being positioned do not fit into
the workspace. We describe three such techniques below,
deferring formal statements of the algorithms to Appendix B.
Pruning Based on Containment. The simplest technique
applies to any objectX whose position is uniform in a region
R and which must be contained in a region C (e.g. the road
in our case study). If minRadius is a lower bound on the
distance from the center of X to its bounding box, then we
can restrict R to R∩erode(C, minRadius). This is sound, since

<!-- page:11 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
if X is centered anywhere not in the restriction, then some
point of its bounding box must lie outside of C.
Pruning Based on Orientation. The next technique ap-
plies to scenarios placing constraints on the relative heading
and the maximum distance M between objects X and Y ,
which are oriented with respect to a vector field that is con-
stant within polygonal regions (such as our roads). For each
polygon P, we find all polygons Qi satisfying the relative
heading constraints with respect to P (up to a perturbation if
X and Y need not be exactly aligned to the field), and restrict
P to P∩ dilate(∪Qi , M). This is also sound: supposeX can be
positioned at x in polygon P. Then Y must lie at somey in a
polygon Q satisfying the constraints, and since the distance
from x toy is at most M, we have x∈ dilate(Q, M).
Pruning Based on Size. Finally, in the setting above of ob-
jects X and Y aligned to a polygonal vector field (with maxi-
mum distance M), we can also prune the space using a lower
bound on the width of the configuration. For example, in our
bumper-to-bumper scenario we can infer such a bound from
the offset by specifiers in the program. We first find all
polygons that are not wide enough to fit the configuration
according to the bound: call these “narrow”. Then we restrict
each narrow polygon P to P∩ dilate(∪Qi , M) where Qi runs
over all polygons except P. To see that this is sound, suppose
object X can lie at x in polygon P. If P is not narrow, we do
not restrict it; otherwise, objectY must lie aty in some other
polygon Q. Since the distance from x toy is at most M, as
above we have x∈ dilate(Q, M).
After pruning the space as described above, our implemen-
tation uses rejection sampling, generating scenes from the
imperative part of the scenario until all requirements are
satisfied. While this samples from exactly the desired distri-
bution, it has the drawback that a huge number of samples
may be required to yield a single valid scene (in the worst
case, when the requirements have probability zero of being
satisfied, the algorithm will not even terminate). However,
we found in our experiments that all reasonable scenarios
we tried required only several hundred iterations at most,
yielding a sample within a few seconds. Furthermore, the
pruning methods above could reduce the number of samples
needed by a factor of 3 or more (see Appendix D for details
of our experiments). In future work it would be interesting to
see whether Markov chain Monte Carlo methods previously
used for probabilistic programming (see, e.g., [ 32, 35, 48])
could be made effective in the case of Scenic.
6 Experiments
We demonstrate the three applications of Scenic discussed
in Sec. 2: testing a system under particular conditions (6.2),
training the system to improve accuracy in hard cases (6.3),
and debugging failures (6.4).
6.1 Experimental Setup
We generated scenes in the virtual world of the video game
Grand Theft Auto V (GTAV) [ 15]. We wrote a Scenic li-
brary gtaLib defining Regions representing the roads and
curbs in (part of) this world, as well as a type of object Car
providing two additional properties3: model, representing
the type of car, with a uniform distribution over 13 diverse
models provided by GTAV, andcolor, representing the car
color, with a default distribution based on real-world car
color statistics [8]. In addition, we implemented two global
scene parameters: time, representing the time of day, and
weather, representing the weather as one of 14 discrete types
supported by GTAV (e.g. “clear” or “snow”).
GTAV is closed-source and does not expose any kind of
scene description language. Therefore, to import scenes gen-
erated by Scenic into GTAV, we wrote a plugin based on
DeepGTAV4. The plugin calls internal functions of GTAV to
create cars with the desired positions, colors, etc., as well as
to set the camera position, time of day, and weather.
Our experiments used squeezeDet [49], a convolutional
neural network real-time object detector for autonomous
driving5. We used a batch size of20 and trained all models for
10,000 iterations unless otherwise noted. Images captured
from GTAV with resolution 1920× 1200 were resized to
1248× 384, the resolution used by squeezeDet. All models
were trained and evaluated on NVIDIA TITAN Xp GPUs.
We used standard metrics precision and recall to measure
the accuracy of detection on a particular image set. The ac-
curacy is computed based on how well the network predicts
the correct bounding box, score, and category of objects in
the image set. Details are in Appendix D, but in brief, pre-
cision is defined as tp/(tp + f p) and recall as tp/(tp + f n),
where true positives tp is the number of correct detections,
false positives f p is the number of predicted boxes that do
not match any ground truth box, and false negatives f n is
the number of ground truth boxes that are not detected.
6.2 Testing under Different Conditions
When testing a model, one may be interested in a particular
operation regime. For instance, an autonomous car manufac-
turer may be more interested in certain road conditions (e.g.
desert vs. forest roads) depending on where its cars will be
mainly used. Scenic provides a systematic way to describe
scenarios of interest and construct corresponding test sets.
To demonstrate this, we first wrote very general scenarios
describing scenes of 1–4 cars (not counting the camera),
specifying only that the cars face within 10◦ of the road
direction. We generated 1,000 images from each scenario,
yielding a training set Xgeneric of 4,000 images, and used
3For the full definition of Car, see Appendix A; the definitions of road,
curb, etc. are a few lines loading the corresponding sets of points from a
file storing the GTAV map (see Appendix D for how this was generated).
4https://github.com/aitorzip/DeepGTAV
5Used industrially, for example by DeepScale (http://deepscale.ai/).

<!-- page:12 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
1 wiggle = (-10 deg, 10 deg)
2 ego = Car with roadDeviation wiggle
3 c = Car visible,
4 with roadDeviation resample(wiggle)
5 leftRight = Uniform(1.0, -1.0) * (1.25, 2.75)
6 Car beyond c by leftRight @ (4, 10),
7 with roadDeviation resample(wiggle)
Figure 8. A scenario where one car partially occludes an-
other. The property roadDeviation is defined in Car to
mean its heading relative to the roadDirection.
these to train a model Mgeneric as described in Sec. 6.1. We
also generated an additional 50 images from each scenario
to obtain a generic test set Tgeneric of 200 images.
Next, we specialized the general scenarios in opposite
directions: scenarios for good/bad road conditions fixing
the time to noon/midnight and the weather to sunny/rainy
respectively, generating specialized test sets Tgood and Tbad.
Evaluating Mgeneric on Tgeneric, Tgood, andTbad, we obtained
precisions of 83.1%, 85.7%, and 72.8%, respectively, and re-
calls of 92.6%, 94.3%, and 92.8%. This shows that, as might
be expected, the model performs better on bright days than
on rainy nights. This suggests there might not be enough ex-
amples of rainy nights in the training set, and indeed under
our default weather distribution rain is less likely than shine.
This illustrates how specialized test sets can highlight the
weaknesses and strengths of a particular model. In the next
section, we go one step further and use Scenic to redesign
the training set and improve model performance.
6.3 Training on Rare Events
In the synthetic data setting, we are limited not by data
availability but by the cost of training. The natural question
is then how to generate a synthetic data set that as effective
as possible given a fixed size. In this section we show that
over-representing a type of input that may occur rarely but is
difficult for the model can improve performance on the hard
case without compromising performance in the typical case.
Scenic makes this possible by allowing the user to write a
scenario capturing the hard case specifically.
For our car detection task, an obvious hard case is when
one car substantially occludes another. We wrote a simple
scenario, shown in Fig. 8, which generates such scenes by
placing one car behind the other as viewed from the camera,
offset left or right so that it is at least partially visible (sample
images are in Appendix A.8). Generating images from this
scenario we obtained a training set Xoverlap of 250 images
and a test set Toverlap of 200 images.
For a baseline training set we used the “Driving in the
Matrix” synthetic data set [ 25], which has been shown to
yield good car detection performance even on real-world
Table 6.Performance of models trained on 5,000 images from
Xmatrix or a mixture with Xoverlap, averaged over 8 training
runs with random selections of images from Xmatrix.
Mixture Tmatrix Toverlap
% Precision Recall Precision Recall
100 / 0 72.9± 3.7 37 .1± 2.1 62 .8± 6.1 65 .7± 4.0
95 / 5 73.1± 2.3 37 .0± 1.6 68 .9± 3.2 67 .3± 2.4
images6. Like our images, the “Matrix” images were rendered
in GTAV; however, rather than using a PPL to guide genera-
tion, they were produced by allowing the game’s AI to drive
around randomly while periodically taking screenshots. We
randomly selected 5,000 of these images to form a training set
Xmatrix, and 200 for a test set Tmatrix. We trained squeezeDet
for 5,000 iterations on Xmatrix, evaluating it on Tmatrix and
Toverlap. To reduce the effect of jitter during training we used
a standard technique [2], saving the last 10 models in steps
of 10 iterations and picking the one achieving the best total
precision and recall. This yielded the results in the first row
of Tab. 6. Although Xmatrix contains many images of over-
lapping cars, the precision on Toverlap is significantly lower
than for Tmatrix, indicating that the network is predicting
lower-quality bounding boxes for such cars7.
Next we attempted to improve the effectiveness of the
training set by mixing in the difficult images produced with
Scenic. Specifically, we replaced a random 5% ofXmatrix (250
images) with images from Xoverlap, keeping the overall train-
ing set size constant. We then retrained the network on the
new training set and evaluated it as above. To reduce the
dependence on which images were replaced, we averaged
over 8 training runs with different random selections of the
250 images to replace. The results are shown in the second
row of Tab. 6. Even altering only 5% of the training set, per-
formance on Toverlap significantly improves. Critically, the
improvement on Toverlap is not paid for by a corresponding
decrease on Tmatrix: performance on the original data set re-
mains the same. Thus, by allowing us to specify and generate
instances of a difficult case, Scenic enables the generation
of more effective training sets than can be obtained through
simpler approaches not based on PPLs.
6We use the “Matrix” data set since it is known to be effective for car
detection and was not designed by us, making the fact that Scenic is able
to improve it more striking. The results of this experiment also hold under
the Average Precision (AP) metric used in [ 25], as well as in a similar
experiment using the Scenic generic two-car scenario from the last section
as the baseline. See Appendix D for details.
7Recall is much higher on Toverlap, meaning the false-negative rate is better;
this is presumably because all the Toverlap images have exactly 2 cars and
are in that sense easier than the Tmatrix images, which can have many cars.

<!-- page:13 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
Table 7. Performance of Mgeneric on different scenarios rep-
resenting variations of a single misclassified image.
Scenario Precision Recall
(1) varying model and color 80.3 100
(2) varying background 50.5 99.3
(3) varying local position, orientation 62.8 100
(4) varying position but staying close 53.1 99.3
(5) any position, same apparent angle 58.9 98.6
(6) any position and angle 67.5 100
(7) varying background, model, color 61.3 100
(8) staying close, same apparent angle 52.4 100
(9) staying close, varying model 58.6 100
6.4 Debugging Failures
In our final experiment, we show howScenic can be used to
generalize a single input on which a model fails, exploring its
neighborhood in a variety of different directions and giving
insight into which features of the scene are responsible for
the failure. The original failure can then be generalized to a
broader scenario describing a class of inputs on which the
model misbehaves, which can in turn be used for retraining.
We selected one scene from our first experiment, consisting
of a single car viewed from behind at a slight angle, which
Mgeneric wrongly classified as three cars (thus having 33.3%
precision and 100% recall). We wrote several scenarios which
left most of the features of the scene fixed but allowed others
to vary. Specifically, scenario (1) varied the model and color
of the car, (2) left the position and orientation of the car
relative to the camera fixed but varied the absolute position,
effectively changing the background of the scene, and (3)
used the mutation feature of Scenic to add a small amount
of noise to the car’s position, heading, and color (see Ap-
pendix A.6 for code and the original misclassified image).
For each scenario we generated 150 images and evaluated
Mgeneric on them. As seen in Tab. 7, changing the model and
color improved performance the most, suggesting they were
most relevant to the misclassification, while local position
and orientation were less important and global position (i.e.
the background) was least important.
To investigate these possibilities further, we wrote a sec-
ond round of variant scenarios, also shown in Tab. 7. The
results confirmed the importance of model and color (com-
pare (2) to (7)), as well as angle (compare (5) to (6)), but also
suggested that being close to the camera could be the rele-
vant aspect of the car’s local position. We confirmed this with
a final round of scenarios (compare (5) and (8)), which also
showed that the effect of car model is small among scenes
where the car is close to the camera (compare (4) and (9)).
Having established that car model, closeness to the cam-
era, and view angle all contribute to poor performance of
Table 8. Performance of Mgeneric after retraining, replacing
10% of Xgeneric with different data.
Replacement Data Precision Recall
Original (no replacement) 82.9 92.7
Classical augmentation 78.7 92.1
Close car 87.4 91.6
Close car at shallow angle 84.0 92.1
the network, we wrote broader scenarios capturing these
features. To avoid overfitting, and since our experiments in-
dicated car model was not very relevant when the car is close
to the camera, we decided not to fix the car model. Instead,
we specialized the generic one-car scenario from our first
experiment to produce only cars close to the camera. We
also created a second scenario specializing this further by
requiring that the car be viewed at a shallow angle.
Finally, we used these scenarios to retrainMgeneric, hoping
to improve performance on its original test set Tgeneric (to
better distinguish small differences in performance, we in-
creased the test set size to 400 images). To keep the size of the
training set fixed as in the previous experiment, we replaced
400 one-car images in Xgeneric (10% of the whole training set)
with images generated from our scenarios. As a baseline,
we used images produced with classical image augmenta-
tion techniques implemented inimgaug [26]. Specifically, we
modified the original misclassified image by randomly crop-
ping 10%–20% on each side, flipping horizontally with prob-
ability 50%, and applying Gaussian blur withσ∈[ 0.0, 3.0].
The results of retraining Mgeneric on the resulting data sets
are shown in Tab. 8. Interestingly, classical augmentation
actually hurt performance, presumably due to overfitting
to relatively slight variants of a single image. On the other
hand, replacing part of the data set with specialized images
of cars close to the camera significantly reduced the number
of false positives like the original misclassification (while
the improvement for the “shallow angle” scenario was less,
perhaps due to overfitting to the restricted angle range). This
demonstrates how Scenic can be used to improve perfor-
mance by generalizing individual failures into scenarios that
capture the essence of the problem but are broad enough to
prevent overfitting during retraining.
7 Related Work
Data Generation and Testing for ML. There has been
a large amount of work on generating synthetic data for
specific applications, including text recognition [23], text lo-
calization [22], robotic object grasping [45], and autonomous
driving [10, 25]. Closely related is work on domain adapta-
tion, which attempts to correct differences between synthetic
and real-world input distributions. Domain adaptation has

<!-- page:14 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
enabled synthetic data to successfully train models for sev-
eral other applications including 3D object detection [29, 43],
pedestrian detection [ 46], and semantic image segmenta-
tion [39]. Such work provides important context for our
paper, showing that models trained exclusively on synthetic
data (possibly domain-adapted) can achieve acceptable per-
formance on real-world data. The major difference in our
work is that we provide, through Scenic, language-based
systematic data generation for any perception system.
Some works have also explored the idea of using adver-
sarial examples (i.e. misclassified examples) to retrain and
improve ML models (e.g., [18, 47, 50]). In particular, Genera-
tive Adversarial Networks (GANs) [17], a particular kind of
neural network able to generate synthetic data, have been
used to augment training sets [28, 30]. The difference with
Scenic is that GANs require an initial training set/pretrained
model and do not easily incorporate declarative constraints,
while Scenic produces synthetic data in an explainable, pro-
grammatic fashion requiring only a simulator.
Model-Based Test Generation. Techniques using a model
to guide test generation have long existed [ 3]. A popular
approach is to provide example tests, as in mutational fuzz
testing [44] and example-based scene synthesis [11]. While
these methods are easy to use, they do not provide fine-
grained control over the generated data. Another approach
is to give rules or a grammar specifying how the data can
be generated, as in generative fuzz testing [44], procedural
generation from shape grammars [33], and grammar-based
scene synthesis [24]. While grammars allow much greater
control, they do not easily allow enforcing global properties.
This is also true when writing aprogram in a domain-specific
language with nondeterminism [9]. Conversely, constraints
as in constrained-random verification [34] allow global prop-
erties but can be difficult to write. Scenic improves on these
methods by simultaneously providing fine-grained control,
enforcement of global properties, specification of probability
distributions, and simple imperative syntax.
Probabilistic Programming Languages. The semantics
(and to some extent, the syntax) of Scenic are similar to
that of other probabilistic programming languages such as
Prob [21], Church [19], and BLOG [32]. In probabilistic pro-
gramming the focus is usually oninference rather than gener-
ation (the main application in our case), and in particular to
our knowledge probabilistic programming languages have
not previously been used for test generation. However, the
most popular inference techniques are based on sampling
and so could be directly applied to generate scenes from
Scenic programs, as we discussed in Sec. 5.
Several probabilistic programming languages have been
used to define generative models of objects and scenes: both
general-purpose languages such as WebPPL [20] (see, e.g.,
[38]) and languages specifically motivated by such applica-
tions, namely Quicksand [37] and Picture [27]. The latter are
in some sense the most closely-related to Scenic, although
neither provides specialized syntax or semantics for dealing
with geometry (Picture also was used only for inverse ren-
dering, not data generation). The main advantage of Scenic
over these languages is that its domain-specific design per-
mits concise representation of complex scenarios and enables
specialized sampling techniques.
8 Conclusion
In this paper, we introduced Scenic, a probabilistic program-
ming language for specifying distributions over configura-
tions of physical objects and agents. We showed howScenic
can be used to generate synthetic data sets useful for deep
learning tasks. Specifically, we used Scenic to generate spe-
cialized test sets, improve the effectiveness of training sets by
emphasizing difficult cases, and generalize from individual
failure cases to broader scenarios suitable for retraining. In
particular, by training on hard cases generated byScenic, we
were able to boost the performance of a car detector neural
network (given a fixed training set size) significantly beyond
what could be achieved by prior synthetic data generation
methods [25] not based on PPLs.
In future work we plan to conduct experiments applying
Scenic to a variety of additional domains, applications, and
simulators. For example, we have integrated Scenic as the
environment modeling language for VerifAI, a tool for the
design and analysis of AI-based systems [7], and used it to
generate seed inputs for temporal-logic falsification of an au-
tomated collision-avoidance system. We have also interfaced
Scenic to the X-Plane flight simulator [36] in order to test
ML-based aircraft navigation systems, and to the CARLA
driving simulator [6] for scenarios requiring more control
than GTAV provides. Finally, we plan to extend theScenic
language itself in several directions: allowing user-defined
specifiers, describing 3D scenes, and encoding dynamic sce-
narios to aid in the analysis of complex dynamic behaviors,
including both control as well as perception.
Acknowledgments
The authors would like to thank Ankush Desai, Alastair
Donaldson, Andrew Gordon, Jonathan Ragan-Kelley, Sriram
Rajamani, and several anonymous reviewers for helpful dis-
cussions and feedback. This work is supported in part by
the National Science Foundation Graduate Research Fellow-
ship Program under Grant No. DGE-1106400, NSF grants
CNS-1545126 (VeHICaL), CNS-1646208, CNS-1739816, and
CCF-1837132, DARPA under agreement number FA8750-16-
C0043, the DARPA Assured Autonomy program, Berkeley
Deep Drive, the iCyPhy center, and TerraSwarm, one of six
centers of STARnet, a Semiconductor Research Corporation
program sponsored by MARCO and DARPA.

<!-- page:15 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
References
[1] Dario Amodei, Chris Olah, Jacob Steinhardt, Paul F. Christiano, John
Schulman, and Dan Mané. 2016. Concrete Problems in AI Safety.CoRR
abs/1606.06565 (2016). arXiv:1606.06565
[2] Sylvain Arlot and Alain Celisse. 2010. A survey of cross-validation
procedures for model selection. Statist. Surv. 4 (2010), 40–79. https:
//doi.org/10.1214/09-SS054
[3] Manfred Broy, Bengt Jonsson, Joost-Pieter Katoen, Martin Leucker, and
Alexander Pretschner. 2005. Model-Based Testing of Reactive Systems:
Advanced Lectures (Lecture Notes in Computer Science). Springer-Verlag
New York, Inc., Secaucus, NJ, USA.
[4] João Cartucho. 2019. mean Average Precision. https://github.com/
Cartucho/mAP.
[5] Guillaume Claret, Sriram K Rajamani, Aditya V Nori, Andrew D Gor-
don, and Johannes Borgström. 2013. Bayesian inference using data flow
analysis. In Proceedings of the 2013 9th Joint Meeting on Foundations of
Software Engineering. ACM, 92–102.
[6] Alexey Dosovitskiy, German Ros, Felipe Codevilla, Antonio Lopez,
and Vladlen Koltun. 2017. CARLA: An Open Urban Driving Simulator.
In Conference on Robot Learning, CoRL . 1–16.
[7] Tommaso Dreossi, Daniel J. Fremont, Shromona Ghosh, Edward Kim,
Hadi Ravanbakhsh, Marcell Vazquez-Chanlatte, and Sanjit A. Seshia.
2019. VerifAI: A Toolkit for the Design and Analysis of Artificial
Intelligence-Based Systems. arXiv:1902.04245 https://github.com/
BerkeleyLearnVerify/VerifAI
[8] DuPont. 2012. Global Automotive Color Popularity Re-
port. https://web.archive.org/web/20130818022236/http:
//www2.dupont.com/Media_Center/en_US/color_popularity/
Images_2012/DuPont2012ColorPopularity.pdf
[9] Tayfun Elmas, Jacob Burnim, George Necula, and Koushik Sen. 2013.
CONCURRIT: a domain specific language for reproducing concurrency
bugs. In ACM SIGPLAN Notices, Vol. 48. ACM, 153–164.
[10] Artur Filipowicz, Jeremiah Liu, and Alain Kornhauser. 2017. Learning
to recognize distance to stop signs using the virtual world of Grand Theft
Auto 5. Technical Report. Princeton University.
[11] Matthew Fisher, Daniel Ritchie, Manolis Savva, Thomas Funkhouser,
and Pat Hanrahan. 2012. Example-based Synthesis of 3D Object Ar-
rangements. In ACM SIGGRAPH 2012 (SIGGRAPH Asia ’12) .
[12] Daniel Fremont, Xiangyu Yue, Tommaso Dreossi, Shromona Ghosh,
Alberto L. Sangiovanni-Vincentelli, and Sanjit A. Seshia. 2018. Scenic:
Language-Based Scene Generation. Technical Report UCB/EECS-2018-8.
EECS Department, University of California, Berkeley. http://www2.
eecs.berkeley.edu/Pubs/TechRpts/2018/EECS-2018-8.html
[13] Daniel J. Fremont, Alexandre Donzé, Sanjit A. Seshia, and David Wessel.
2015. Control Improvisation. In 35th IARCS Annual Conference on
Foundation of Software Technology and Theoretical Computer Science
(FSTTCS) (LIPIcs), Vol. 45. 463–474.
[14] Daniel J. Fremont, Tommaso Dreossi, Shromona Ghosh, Xiangyu
Yue, Alberto L. Sangiovanni-Vincentelli, and Sanjit A. Seshia. 2019.
Scenic: A Language for Scenario Specification and Scene Generation.
arXiv:1809.09310 https://github.com/BerkeleyLearnVerify/Scenic
[15] Rockstar Games. 2015. Grand Theft Auto V. Windows PC version.
https://www.rockstargames.com/games/info/V
[16] Adele Goldberg and David Robson. 1983. Smalltalk-80: The Language
and its Implementation. Addison-Wesley, Reading, Massachusetts.
[17] Ian Goodfellow, Jean Pouget-Abadie, Mehdi Mirza, Bing Xu, David
Warde-Farley, Sherjil Ozair, Aaron Courville, and Yoshua Bengio. 2014.
Generative adversarial nets. In Advances in neural information process-
ing systems. 2672–2680.
[18] Ian J. Goodfellow, Jonathon Shlens, and Christian Szegedy. 2014. Ex-
plaining and Harnessing Adversarial Examples. CoRR abs/1412.6572
(2014). arXiv:1412.6572
[19] Noah Goodman, Vikash K. Mansinghka, Daniel Roy, Keith Bonawitz,
and Joshua B. Tenenbaum. 2008. Church: A universal language for
generative models. In Uncertainty in Artificial Intelligence 24 (UAI) .
220–229.
[20] Noah D Goodman and Andreas Stuhlmüller. 2014. The Design and
Implementation of Probabilistic Programming Languages. http://dippl.
org. Accessed: 2018-7-11.
[21] Andrew D Gordon, Thomas A Henzinger, Aditya V Nori, and Sriram K
Rajamani. 2014. Probabilistic programming. In FOSE 2014. ACM, 167–
181.
[22] Ankush Gupta, Andrea Vedaldi, and Andrew Zisserman. 2016. Syn-
thetic Data for Text Localisation in Natural Images. InComputer Vision
and Pattern Recognition, CVPR . 2315–2324. https://doi.org/10.1109/
CVPR.2016.254
[23] Max Jaderberg, Karen Simonyan, Andrea Vedaldi, and Andrew Zisser-
man. 2014. Synthetic Data and Artificial Neural Networks for Natural
Scene Text Recognition. CoRR abs/1406.2227 (2014). arXiv:1406.2227
[24] Chenfanfu Jiang, Siyuan Qi, Yixin Zhu, Siyuan Huang, Jenny Lin, Lap-
Fai Yu, Demetri Terzopoulos, and Song-Chun Zhu. 2018. Configurable
3D Scene Synthesis and 2D Image Rendering with Per-pixel Ground
Truth Using Stochastic Grammars. International Journal of Computer
Vision (2018), 1–22.
[25] Matthew Johnson-Roberson, Charles Barto, Rounak Mehta,
Sharath Nittur Sridhar, Karl Rosaen, and Ram Vasudevan.
2017. Driving in the Matrix: Can virtual worlds replace
human-generated annotations for real world tasks?. In Interna-
tional Conference on Robotics and Automation, ICRA . 746–753.
https://doi.org/10.1109/ICRA.2017.7989092
[26] Alexander Jung. 2018. imgaug. https://github.com/aleju/imgaug
[27] Tejas Kulkarni, Pushmeet Kohli, Joshua B. Tenenbaum, and Vikash K.
Mansinghka. 2015. Picture: A probabilistic programming language for
scene perception. In IEEE Conference on Computer Vision and Pattern
Recognition (CVPR). 4390–4399.
[28] Xiaodan Liang, Zhiting Hu, Hao Zhang, Chuang Gan, and Eric P
Xing. 2017. Recurrent Topic-Transition GAN for Visual Paragraph
Generation. arXiv preprint arXiv:1703.07022 (2017).
[29] Joerg Liebelt and Cordelia Schmid. 2010. Multi-view object class
detection with a 3D geometric model. In Computer Vision and Pattern
Recognition, CVPR. 1688–1695. https://doi.org/10.1109/CVPR.2010.
5539836
[30] Marco Marchesi. 2017. Megapixel Size Image Creation using Genera-
tive Adversarial Networks. arXiv preprint arXiv:1706.00082 (2017).
[31] Olivier Michel. 2004. Webots: Professional Mobile Robot Simulation.
International Journal of Advanced Robotic Systems 1, 1 (2004), 39–42.
[32] Brian Milch, Bhaskara Marthi, and Stuart Russell. 2004. BLOG: Re-
lational modeling with unknown objects. In ICML 2004 workshop on
statistical relational learning and its connections to other fields . 67–73.
[33] Pascal Müller, Peter Wonka, Simon Haegler, Andreas Ulmer, and Luc
Van Gool. 2006. Procedural modeling of buildings. InACM Transactions
On Graphics, Vol. 25. ACM, 614–623.
[34] Yehuda Naveh, Michal Rimon, Itai Jaeger, Yoav Katz, Michael Vinov,
Eitan Marcus, and Gil Shurek. 2006. Constraint-Based Random Stimuli
Generation for Hardware Verification. In Proc. of AAAI. 1720–1727.
[35] Aditya V Nori, Chung-Kil Hur, Sriram K Rajamani, and Selva Samuel.
2014. R2: An Efficient MCMC Sampler for Probabilistic Programs.. In
AAAI. 2476–2482.
[36] Laminar Research. 2019. X-Plane 11. https://www.x-plane.com/
[37] Daniel Ritchie. 2014. Quicksand: A Lightweight Embedding of Proba-
bilistic Programming for Procedural Modeling and Design. In 3rd NIPS
Workshop on Probabilistic Programming. https://dritchie.github.io/pdf/
qs.pdf
[38] Daniel Ritchie. 2016. Probabilistic programming for procedural modeling
and design . Ph.D. Dissertation. Stanford University. https://purl.
stanford.edu/vh730bw6700
[39] Germán Ros, Laura Sellart, Joanna Materzynska, David Vázquez, and
Antonio M. López. 2016. The SYNTHIA Dataset: A Large Collection

<!-- page:16 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
of Synthetic Images for Semantic Segmentation of Urban Scenes. In
Computer Vision and Pattern Recognition, CVPR . 3234–3243. https:
//doi.org/10.1109/CVPR.2016.352
[40] Stuart Russell, Tom Dietterich, Eric Horvitz, Bart Selman, Francesca
Rossi, Demis Hassabis, Shane Legg, Mustafa Suleyman, Dileep George,
and Scott Phoenix. 2015. Letter to the Editor: Research Priorities
for Robust and Beneficial Artificial Intelligence: An Open Letter. AI
Magazine 36, 4 (2015).
[41] Nasser Saheb-Djahromi. 1978. Probabilistic LCF. In Mathematical
Foundations of Computer Science . Springer, 442–451.
[42] Sanjit A. Seshia, Dorsa Sadigh, and S. Shankar Sastry. 2016. Towards
Verified Artificial Intelligence. arXiv:1606.08514
[43] Michael Stark, Michael Goesele, and Bernt Schiele. 2010. Back to the
Future: Learning Shape Models from 3D CAD Data. In British Machine
Vision Conference, BMVC. 1–11. https://doi.org/10.5244/C.24.106
[44] Michael Sutton, Adam Greene, and Pedram Amini. 2007. Fuzzing:
Brute Force Vulnerability Discovery. Addison-Wesley.
[45] Josh Tobin, Rachel Fong, Alex Ray, Jonas Schneider, Wojciech Zaremba,
and Pieter Abbeel. 2017. Domain randomization for transferring deep
neural networks from simulation to the real world. In International
Conference on Intelligent Robots and Systems, IROS . 23–30. https:
//doi.org/10.1109/IROS.2017.8202133
[46] David Vazquez, Antonio M Lopez, Javier Marin, Daniel Ponsa, and
David Geronimo. 2014. Virtual and real world adaptation for pedestrian
detection. IEEE transactions on pattern analysis and machine intelligence
36, 4 (2014), 797–809.
[47] Sebastien C Wong, Adam Gatt, Victor Stamatescu, and Mark D McDon-
nell. 2016. Understanding data augmentation for classification: when
to warp?. In Digital Image Computing: Techniques and Applications
(DICTA), 2016 International Conference on . IEEE, 1–6.
[48] Frank Wood, Jan Willem Meent, and Vikash Mansinghka. 2014. A
new approach to probabilistic programming inference. In Artificial
Intelligence and Statistics. 1024–1032.
[49] Bichen Wu, Forrest N. Iandola, Peter H. Jin, and Kurt Keutzer. 2017.
SqueezeDet: Unified, Small, Low Power Fully Convolutional Neural
Networks for Real-Time Object Detection for Autonomous Driving.
In Conference on Computer Vision and Pattern Recognition Workshops,
CVPR Workshops. 446–454. https://doi.org/10.1109/CVPRW.2017.60
[50] Yan Xu, Ran Jia, Lili Mou, Ge Li, Yunchuan Chen, Yangyang Lu, and
Zhi Jin. 2016. Improved relation classification by deep recurrent neural
networks with data augmentation. arXiv preprint arXiv:1601.03651
(2016).

<!-- page:17 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A Gallery of Scenarios
This section presents Scenic code for a variety of scenarios
from our autonomous car case study (and the robot motion
planning example used in Sec. 3), along with images rendered
from them. The scenarios range from simple examples used
above to illustrate different aspects of the language, to those
representing interesting road configurations like platoons
and lanes of traffic.
Contents
A.1 The gtaLib Module 17
A.2 The Simplest Possible Scenario 18
A.3 A Single Car 19
A.4 A Badly-Parked Car 20
A.5 An Oncoming Car 21
A.6 Adding Noise to a Scene 22
A.7 Two Cars 23
A.8 Two Overlapping Cars 24
A.9 Four Cars, in Poor Driving Conditions 25
A.10 A Platoon, in Daytime 26
A.11 Bumper-to-Bumper Traffic 27
A.12 Robot Motion Planning with a Bottleneck 29
A.1 The gtaLib Module
All the scenarios below begin with a line (not shown here)
importing the gtaLib module, which as explained above
contains all definitions specific to our autonomous car case
study. These include the definitions of the regions road and
curb, as well as the vector field roadDirection giving the
prevailing traffic direction at each point on the road. Most
importantly, it also defines Car as a type of object:
1 class Car:
2 position: Point on road
3 heading: (roadDirection at self.position) \
4 + self.roadDeviation
5 roadDeviation: 0
6 width: self.model.width
7 height: self.model.height
8 viewAngle: 80 deg
9 visibleDistance: 30
10 model: CarModel.defaultModel()
11 color: CarColor.defaultColor()
Most of the properties are inherited from Object or are
self-explanatory. The property roadDeviation, represent-
ing the heading of the car with respect to the local direction
of the road, is purely a syntactic convenience; the following
two lines are equivalent:
1 Car facing 10 deg relative to roadDirection
2 Car with roadDeviation 10 deg
The gtaLib library also defines a few convenience sub-
classes of Car with different default properties. For example,
EgoCar overrides model with the fixed car model we used
for the ego car in our interface to GTA V.

<!-- page:18 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
A.2 The Simplest Possible Scenario
This scenario, creating a single car with no specified proper-
ties, was used as an example in Sec. 3.
1 ego = Car
2 Car
Figure 9. Scenes generated from a Scenic scenario representing a single car (with reasonable default properties).

<!-- page:19 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.3 A Single Car
This scenario is slightly more general than the previous, al-
lowing the car (and the ego car) to deviate from the road
direction by up to 10◦. It also specifies that the car must be
visible, which is in fact redundant since this constraint is
built into Scenic, but helps guide the sampling procedure.
This scenario was also used as an example in Sec. 3.
1 wiggle = (-10 deg, 10 deg)
2 ego = EgoCar with roadDeviation wiggle
3 Car visible, with roadDeviation resample(wiggle)
Figure 10. Scenes generated from a Scenic scenario representing a single car facing roughly the road direction.

<!-- page:20 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
A.4 A Badly-Parked Car
This scenario, creating a single car parked near the curb but
not quite parallel to it, was used as an example in Sec. 3.
1 ego = Car
2 spot = OrientedPoint on visible curb
3 badAngle = Uniform(1.0, -1.0) * (10, 20) deg
4 Car left of spot by 0.5, facing badAngle relative to roadDirection
Figure 11. Scenes generated from a Scenic scenario representing a badly-parked car.

<!-- page:21 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.5 An Oncoming Car
This scenario, creating a car 20–40 m ahead and roughly
facing towards the camera, was used as an example in Sec. 3.
Note that since we do not specify the orientation of the car
when creating it, the default heading is used and so it will
face the road direction. Therequire statement then requires
that this orientation is also within 15◦ of facing the camera
(as the view cone is 30◦ wide).
1 ego = Car
2 car2 = Car offset by (-10, 10) @ (20, 40), with viewAngle 30 deg
3 require car2 can see ego
Figure 12. Scenes generated from a Scenic scenario representing a car facing roughly towards the camera.

<!-- page:22 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
A.6 Adding Noise to a Scene
This scenario, using Scenic’s mutation feature to automati-
cally add noise to an otherwise completely-specified scenario,
was used in the experiment in Sec. 6.4 (it is Scenario (3) in
Table 7). The original scene, which is exactly reproduced by
this scenario if the mutate statement is removed, is shown
in Fig. 14.
1 param time = 12 * 60 # noon
2 param weather = 'EXTRASUNNY'
3
4 ego = EgoCar at -628.7878 @ -540.6067,
5 facing -359.1691 deg
6
7 Car at -625.4444 @ -530.7654, facing 8.2872 deg,
8 with model CarModel.models[ 'DOMINATOR'],
9 with color CarColor.byteToReal([187, 162, 157])
10
11 mutate
Figure 14. The original misclassified image in Sec. 6.4.
Figure 13. Scenes generated from a Scenic scenario adding noise to the scene in Fig. 14.

<!-- page:23 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.7 Two Cars
This is the generic two-car scenario used in the experiments
in Secs. 6.2 and 6.3.
1 wiggle = (-10 deg, 10 deg)
2 ego = EgoCar with roadDeviation wiggle
3 Car visible, with roadDeviation resample(wiggle)
4 Car visible, with roadDeviation resample(wiggle)
Figure 15. Scenes generated from a Scenic scenario representing two cars, facing close to the direction of the road.

<!-- page:24 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
A.8 Two Overlapping Cars
This is the scenario used to produce images of two partially-
overlapping cars for the experiment in Sec. 6.3.
1 wiggle = (-10 deg, 10 deg)
2 ego = EgoCar with roadDeviation wiggle
3
4 c = Car visible, with roadDeviation resample(wiggle)
5
6 leftRight = Uniform(1.0, -1.0) * (1.25, 2.75)
7 Car beyond c by leftRight @ (4, 10), with roadDeviation resample(wiggle)
Figure 16. Scenes generated from a Scenic scenario representing two cars, one partially occluding the other.

<!-- page:25 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.9 Four Cars, in Poor Driving Conditions
This is the scenario used to produce images of four cars in
poor driving conditions for the experiment in Sec. 6.2. With-
out the first two lines, it is the generic four-car scenario used
in that experiment.
1 param weather = 'RAIN'
2 param time = 0 * 60 # midnight
3
4 wiggle = (-10 deg, 10 deg)
5 ego = EgoCar with roadDeviation wiggle
6 Car visible, with roadDeviation resample(wiggle)
7 Car visible, with roadDeviation resample(wiggle)
8 Car visible, with roadDeviation resample(wiggle)
9 Car visible, with roadDeviation resample(wiggle)
Figure 17. Scenes generated from a Scenic scenario representing four cars in poor driving conditions.

<!-- page:26 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
A.10 A Platoon, in Daytime
This scenario illustrates how Scenic can construct struc-
tured object configurations, in this case a platoon of cars.
It uses a helper function provided by gtaLib for creating
platoons starting from a given car, shown in Fig. 18. If no
argument model is provided, as in this case, all cars in the
platoon have the same model as the starting car; otherwise,
the given model distribution is sampled independently for
each car. The syntax for functions and loops supported by
our Scenic implementation is inherited from Python.
1 param time = (8, 20) * 60 # 8 am to 8 pm
2 ego = Car with visibleDistance 60
3 c2 = Car visible
4 platoon = createPlatoonAt(c2, 5, dist=(2, 8))
1 def createPlatoonAt(car, numCars, model=None, dist=(2, 8), shift=(-0.5, 0.5), wiggle=0):
2 lastCar = car
3 for i in range(numCars-1):
4 center = follow roadDirection from (front of lastCar) for resample(dist)
5 pos = OrientedPoint right of center by shift,
6 facing resample(wiggle) relative to roadDirection
7 lastCar = Car ahead of pos, with model (car.model if model is None else resample(model))
Figure 18. Helper function for creating a platoon starting from a given car.
Figure 19. Scenes generated from a Scenic scenario representing a platoon of cars during daytime.

<!-- page:27 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.11 Bumper-to-Bumper Traffic
This scenario creates an even more complex type of object
structure, namely three lanes of traffic. It uses the helper
function createPlatoonAt discussed above, plus another
for placing a car ahead of a given car with a specified gap in
between, shown in Fig. 20.
1 depth = 4
2 laneGap = 3.5
3 carGap = (1, 3)
4 laneShift = (-2, 2)
5 wiggle = (-5 deg, 5 deg)
6 modelDist = CarModel.defaultModel()
7
8 def createLaneAt(car):
9 createPlatoonAt(car, depth, dist=carGap, wiggle=wiggle, model=modelDist)
10
11 ego = Car with visibleDistance 60
12 leftCar = carAheadOfCar(ego, laneShift + carGap, offsetX=-laneGap, wiggle=wiggle)
13 createLaneAt(leftCar)
14
15 midCar = carAheadOfCar(ego, resample(carGap), wiggle=wiggle)
16 createLaneAt(midCar)
17
18 rightCar = carAheadOfCar(ego, resample(laneShift) + resample(carGap), offsetX=laneGap, wiggle=wiggle)
19 createLaneAt(rightCar)
1 def carAheadOfCar(car, gap, offsetX=0, wiggle=0):
2 pos = OrientedPoint at (front of car) offset by (offsetX @ gap),
3 facing resample(wiggle) relative to roadDirection
4 return Car ahead of pos
Figure 20. Helper function for placing a car ahead of a car,
with a specified gap in between.

<!-- page:28 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Figure 21. Scenes generated from a Scenic scenario representing bumper-to-bumper traffic.

<!-- page:29 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
A.12 Robot Motion Planning with a Bottleneck
This scenario illustrates the use ofScenic in another domain
(motion planning) and with another simulator (Webots [31]).
Figure 22 encodes a scenario representing a rubble field of
rocks and pipes with a bottleneck between a robot and its
goal that forces the path planner to consider climbing over a
rock. The code is broken into four parts: first, we import a
small library defining the workspace and the types of objects,
then create the robot at a fixed position and the goal (repre-
sented by a flag) at a random position on the other side of
the workspace. Second, we pick a position for the bottleneck,
requiring it to lie roughly on the way from the robot to its
goal, and place a rock there. Third, we position two pipes of
varying lengths which the robot cannot climb over on either
side of the bottleneck, with their ends far enough apart for
the robot to be able to pass between. Finally, to make the
scenario slightly more interesting we add several additional
obstacles, positioned either on the far side of the bottleneck
or anywhere at random. Several resulting workspaces are
shown in Fig. 23.
1 import mars
2 ego = Rover at 0 @ -2
3 goal = Goal at (-2, 2) @ (2, 2.5)
4
5 halfGapWidth = (1.2 * ego.width) / 2
6 bottleneck = OrientedPoint offset by (-1.5, 1.5) @ (0.5, 1.5), facing (-30, 30) deg
7 require abs((angle to goal) - (angle to bottleneck)) <= 10 deg
8 BigRock at bottleneck
9
10 leftEnd = OrientedPoint left of bottleneck by halfGapWidth,
11 facing (60, 120) deg relative to bottleneck
12 rightEnd = OrientedPoint right of bottleneck by halfGapWidth,
13 facing (-120, -60) deg relative to bottleneck
14 Pipe ahead of leftEnd, with height (1, 2)
15 Pipe ahead of rightEnd, with height (1, 2)
16
17 BigRock beyond bottleneck by (-0.5, 0.5) @ (0.5, 1)
18 BigRock beyond bottleneck by (-0.5, 0.5) @ (0.5, 1)
19 Pipe
20 Rock
21 Rock
22 Rock
Figure 22. A Scenic representing rubble fields with a bottleneck so that the direct route to the goal requires climbing over
rocks.

<!-- page:30 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Figure 23. Workspaces generated from the scenario in Fig. 22, viewed in Webots from a fixed camera.

<!-- page:31 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
B Semantics of Scenic
In this section we give a precise semantics for Scenic expressions and statements, building up to a semantics for a complete
program as a distribution over scenes.
B.1 Notation for State and Semantics
We will precisely define the meaning of Scenic language constructs by giving a small-step operational semantics. We will
focus on the aspects of Scenic that set it apart from ordinary imperative languages, skipping standard inference rules for
sequential composition, arithmetic operations, etc. that we essentially use without change. In rules for statements, we will
denote a state of a Scenic program by⟨s,σ ,π ,O⟩, where s is the statement to be executed,σ is the current variable assignment
(a map from variables to values),π is the current global parameter assignment (for param statements), andO is the set of all
objects defined so far. In rules for expressions, we use the same notation, although we sometimes suppress the state on the
right-hand side of rules for expressions without side effects:⟨e,σ ,π ,O⟩→ v means that in the state(σ ,π ,O), the expression
e evaluates to the valuev without side effects.
Since Scenic is a probabilistic programming language, a single expression can be evaluated different ways with different
probabilities. Following the notation of [5, 41], we write→p for a rewrite rule that fires with probability p (probability density
p, in the case of continuous distributions). We will discuss the meaning of such rules in more detail below.
B.2 Semantics of Expressions
As explained in the previous section, Scenic’s expressions are straightforward except for distributions and object definitions.
As in a typical imperative probabilistic programming language, a distribution evaluates to a sample from the distribution,
following the first rule in Fig. 24. For example, if baseDist is a uniform interval distribution and the parameters evaluate to
low = 0 and high = 1, then the distribution can evaluate to any value in[0, 1] with probability density 1.
The semantics of object definitions are given by the second rule in Fig. 24. First note the side effect, namely adding the
newly-defined object to the setO. The premises of the rule describe the procedure for combining the specifiers to obtain the
overall set of properties for the object. The main step is working out the evaluation order for the specifiers so that all their
dependencies are satisfied, as well as deciding for each specifier which properties it should specify (if it specifies a property
optionally, another specifier could take precedence). This is done by the procedure resolveSpecifiers, shown formally as Alg. 1
and which essentially does the following:
Let P be the set of properties defined in the object’s class and superclasses, together with any properties specified by any
of the specifiers. The object will have exactly these properties, and the value of each p∈ P is determined as follows. If p is
specified non-optionally by multiple specifiers the scenario is ill-formed. If p is only specified optionally, and by multiple
specifiers, this is ambiguous and we also declare the scenario ill-formed. Otherwise, the value of p will be determined by its
unique non-optional specifier, unique optional specifier, or the most-derived default value, in that order: call this specifier sp.
Construct a directed graph with vertices P and edges to p from each of the dependencies of sp (if a dependency is not in P,
Distributions
⟨params,σ ,π ,O⟩→ θ v ∈ dom baseDist(θ)
⟨baseDist(params),σ ,π ,O⟩→ Pθ(v)v
Object Definitions
resolveSpecifiers(class, specifiers) =((s1, p1), . . . ,(sn, pn))
⟨s1,σ[self/⊥],π ,O⟩→ r1⟨v1,σ1,π ,O1⟩
⟨s2,σ1[self.p1/v1(p1)],π ,O1⟩→ r2⟨v2,σ2,π ,O2⟩
...
⟨sn,σn−1[self.pn−1/vn−1(pn−1)],π ,On−1⟩→ rn⟨vn,σn,π ,On⟩
inst = newInstance(class,σn[self.pn/vn(pn)](self))
⟨class specifiers ,σ ,π ,O⟩→ r1 ... rn⟨inst,σ ,π ,On∪{ inst}⟩
‘with’ specifier
⟨E,σ ,π ,O⟩→⟨ v,σ ,π ,O′⟩
⟨with property E,σ ,π ,O⟩→{ property7→v}
‘facing vectorField’ specifier
⟨vectorField,σ ,π ,O⟩→ v ⟨self.position,σ ,π ,O⟩→ p
⟨facing vectorField,σ ,π ,O⟩→{ heading7→v(p)}
Figure 24. Semantics of expressions (excluding operators, defined in Appendix C), and two example specifiers. Here baseDist
is viewed as a function mapping parametersθ to a distribution with density function Pθ , and newInstance(class, props) creates
a new instance of a class with the given property values.

<!-- page:32 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Algorithm 1 resolveSpecifiers(class, specifiers)
▷ gather all specified properties
1: specForProperty←∅
2: optionalSpecsForProperty←∅
3: for all specifiers S in specifiers do
4: for all properties P specified non-optionally by S do
5: if P∈ dom specForProperty then
6: syntax error: property P specified twice
7: specForProperty(P)← S
8: for all properties P specified optionally by S do
9: optionalSpecsForProperty(P).append(S)
▷ filter optional specifications
10: for all properties P∈ dom optionalSpecsForProperty do
11: if P∈ dom specForProperty then
12: continue
13: if|optionalSpecsForProperty(P)| > 1 then
14: syntax error: property P specified twice
15: specForProperty(P)← optionalSpecsForProperty(P)[0]
▷ add default specifiers as needed
16: defaults← defaultValueExpressions(class)
17: for all properties P∈ dom defaults do
18: if P < dom specForProperty then
19: specForProperty(P)← defaults(P)
▷ build dependency graph
20: G← empty graph on dom specForProperty
21: for all specifiers S∈ dom specForProperty do
22: for all dependencies D of S do
23: if D < dom specForProperty then
24: syntax error: missing property D required by S
25: add an edge in G from specForProperty(D) to S
26: if G is cyclic then
27: syntax error: specifiers have cyclic dependencies
▷ construct specifier and property evaluation order
28: specsAndProps← empty list
29: for all specifiers S in G in topological order do
30: specsAndProps.append((S,{P| specForProperty(P) = S}))
31: return specsAndProps
then a specifier references a nonexistent property and the scenario is ill-formed). If this graph has a cycle, there are cyclic
dependencies and the scenario is ill-formed (e.g. Car left of 0 @ 0, facing roadDirection : the heading must be known
to evaluate left of vector, but facing vectorField needs position to determine heading). Otherwise, topologically sorting
the graph yields an evaluation order for the specifiers so that all dependencies are available when needed.
The rest of the rule in Fig. 24 simply evaluates the specifiers in this order, accumulating the results as properties of self so
they are available to the next specifier, finally creating the new object once all properties have been assigned. Note that we also
accumulate the probabilities of each specifier’s evaluation, since specifiers are allowed to introduce randomness themselves
(e.g. the on region specifier returns a random point in the region).
As noted above the semantics of the individual specifiers are mostly straightforward, and exact definitions are given in
Appendix C. To illustrate the pattern we precisely define two specifiers in Fig. 24: the with property value specifier, which
has no dependencies but can specify any property, and the facing vectorField specifier, which depends on position and
specifies heading. Both specifiers evaluate to maps assigning a value to each property they specify.

<!-- page:33 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
V ariable/Parameter Assignments
⟨E,σ ,π ,O⟩→⟨ v,σ ,π ,O′⟩
⟨x = E,σ ,π ,O⟩→⟨ pass,σ[x/v],π ,O′⟩
⟨param x = E,σ ,π ,O⟩→⟨ pass,σ ,π[x/v],O′⟩
Hard Reqirements
⟨B,σ ,π ,O⟩→⟨ True,σ ,π ,O′⟩
⟨require B,σ ,π ,O⟩→⟨ pass,σ ,π ,O′⟩
Soft Reqirements
⟨require[p] B,σ ,π ,O⟩→ p⟨require B,σ ,π ,O⟩
⟨require[p] B,σ ,π ,O⟩→ 1−p⟨pass,σ ,π ,O⟩
Mutations
⟨mutate obji by s,σ ,π ,O⟩→⟨ pass,σ ,π ,O[σ(obji).mutationScale/s]⟩
Termination, Step 1: Apply Mutations
O ={o1, . . . ,on} ∀i∈{ 1, . . . ,n} :
Si =O(oi .mutationScale) psi =O(oi .positionStdDev) hsi =O(oi .headingStdDev)
⟨Normal(0, Si· psi ),σ ,π ,O⟩→ ri, x⟨ni,x ,σ ,π ,O⟩
⟨Normal(0, Si· psi ),σ ,π ,O⟩→ ri,y⟨ni,y,σ ,π ,O⟩ ⟨ Normal(0, Si· hsi ),σ ,π ,O⟩→ ri,h⟨ni,h,σ ,π ,O⟩
posi =O(oi .position) +(ni,x , ni,y) headi =O(oi .heading) + ni,h
⟨pass,σ ,π ,O⟩→ r0, x r0,y r0,h ... ⟨Done,σ ,π ,O[oi .position/posi][oi .heading/headi]⟩
Termination, Step 2: Check Default Reqirements
O ={o1, . . . ,on} ∀i : boundingBox(oi)⊆ workspace
∀i , j : oi .allowCollisions∨ oj .allowCollisions∨ boundingBox(oi)∩ boundingBox(oj) =∅
∀i :¬oi .requireVisible∨⟨ ego can see oi ,σ ,π ,O⟩→ True
⟨Done,σ ,π ,O⟩→( π ,O)
Figure 25. Semantics of statements (excluding class definitions and standard rules for sequential composition). Done denotes
a special state ready for the final termination rule to run.
B.3 Semantics of Statements
The semantics of class and object definitions have been discussed above, while rules for the other statements are given in
Fig. 25. As can be seen from the first rule, variable assignment behaves in the standard way. Parameter assignment is nearly
identical, simply updating the global parameter assignmentπ instead of the variable assignmentσ.
As noted above, the require boolean statement is equivalent to an observe in other languages, and following [5] we model
it by allowing the “Hard Requirement” rule in Fig. 25 to only fire when the condition is satisfied (then turning the requirement
into a no-op). If the condition is not satisfied, no rules apply and the program fails to terminate normally. When defining the
semantics of entire Scenic scenarios below we will discard such non-terminating executions, yielding a distribution only over
executions where all hard requirements are satisfied.
The statement require[p] boolean requires only that its condition hold with at least probability p. There are a number
of ways the semantics of such a soft requirement could be defined: we choose the natural definition that require[p] B is
equivalent to a hard requirement require B that is only enforced with probabilityp. This is reflected in the two corresponding
rules in Fig. 25, and clearly ensures that the requirement B will hold with probability at least p, as desired.
Since the mutation statement mutate instance, . . . by number only causes noise to be added at the end of execution, as
discussed above, its rule Fig. 25 simply sets a property on the object(s) indicating that mutation is enabled (and giving the
scale of noise to be added). The noise is actually added by the first of two special rules that apply only once the program has
been reduced to pass and so computation has finished. This rule first looks up the values of the properties mutationScale,
positionStdDev, and headingStdDev for each object. Respectively, these specify the overall scale of the noise to add (by
default zero, i.e. mutation is disabled) and factors allowing the standard deviation for position and heading to be adjusted
individually. The rule then independently samples Gaussian noise with the desired standard deviation for each object and adds
it to the position and heading properties.
Finally, after mutations are applied, the last rule in Fig. 25 checks Scenic’s three built-in hard requirements. Similarly to the
rule for hard requirements, this last rule can only fire if all the built-in requirements are satisfied, otherwise preventing the

<!-- page:34 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
Algorithm 2 pruneByHeading(map, A, M, δ)
1: map’←∅
2: for all polygons P in map do
3: for all polygons Q in map do
4: Q′← dilate(Q, M)
5: if P∩ Q′ ,∅∧ relHead(P, Q)± 2δ∈ A then
6: map’← map’∪( Q′∩ P)
7: return map’
Algorithm 3 pruneByWidth(map, M, minWidth)
1: narrowPolys← narrow(map, minW idth)
2: map’← map\ narrowPolys
3: for all polygons P in narrowPolys do
4: U← Ð
Q∈map\{ P} dilate(Q, M)
5: map’← map’∪( P∩ U)
6: return map’
program from terminating. If the rule does fire, the final result is the output of the scenario: the assignmentπ to the global
parameters, and the setO of all defined objects.
B.4 Semantics of a Scenic Program
As we have just defined it, every time one runs a Scenic program its output is a scene consisting of an assignment to all
the properties of each Object defined in the scenario, plus any global parameters defined with param. Since Scenic allows
sampling from distributions, the imperative part of a scenario actually induces a distribution over scenes, resulting from the
probabilistic rules of the semantics described above. Specifically, for any execution trace the product of the probabilities of
all rewrite rules yields a probability (density) for the trace (see e.g. [5]). The declarative part of a scenario, consisting of its
require statements, modifies this distribution. As mentioned above, hard requirements are equivalent to “observations” in
other probabilistic programming languages, conditioning the distribution on the requirement being satisfied. In particular, if we
discard all traces which do not terminate (due to violating a requirement), then normalizing the probabilities of the remaining
traces yields a distribution over traces, and therefore scenes, that satisfy all our requirements. This is the distribution defined
by the Scenic scenario.
B.5 Sampling Algorithms
This section gives pseudocode for the domain-specific sampling techniques described in Sec. 5.2. Algorithm 2 implements
pruning by orientation, pruning a set of polygons map given an allowed range of relative headings A, a distance bound M, and
a boundδ on the heading deviation between an object and the vector field at its position.
Algorithm 3 similarly implements pruning by size, given map and M as above, plus a bound minWidth on the minimum
width of the configuration. Here the subroutine narrow finds all polygons which are thinner than this bound.

<!-- page:35 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
C Detailed Semantics of Specifiers and Operators
This section provides precise semantics for Scenic’s specifiers and operators, which were informally defined above.
Contents
C.1 Notation 35
C.2 Specifiers for position 35
C.3 Specifiers for position and optionally heading 37
C.4 Specifiers for heading 37
C.5 Operators 38
C.1 Notation
Since none of the specifiers and operators have side effects, to simplify notation we write⟦X ⟧ for the value of the expression X
in the current state (rather than giving inference rules). Throughout this section, S indicates a scalar, V a vector, H a heading,
F a vectorField, R a region, P a Point, and OP an OrientedPoint. Figure 26 defines notation used in the rest of the semantics.
In forwardEuler, N is an implementation-defined parameter specifying how many steps should be used for the forward Euler
approximation when following a vector field (we used N = 4).
⟨x,y⟩ = point with the given XY coordinates
rotate(⟨x,y⟩ ,θ) =⟨x cosθ−y sinθ, x sinθ +y cosθ⟩
offsetLocal(OP ,v) = ⟦OP.position⟧ + rotate(v, ⟦OP.heading⟧)
Disc(c, r) = set of points in the disc centered at c and with radius r
Sector(c, r , h, a) = set of points in the sector of Disc(c, r) centered along h and with angle a
boundingBox(O) = set of points in the bounding box of object O
visibleRegion(X) =
 

Sector(⟦X.position⟧, ⟦X.viewDistance⟧,
⟦X.heading⟧, ⟦X.viewAngle⟧) X∈ OrientedPoint
Disc(⟦X.position⟧, ⟦X.viewDistance⟧) X∈ Point
orientation(R) = preferred orientation of R if any; otherwise⊥
uniformPointIn(R) = a uniformly random point in R
forwardEuler(x, d, F) = result of iterating the map x7→ x + rotate(⟨0, d/N⟩ , ⟦F ⟧(x)) a total of N times on x
Figure 26. Notation used to define the semantics.
C.2 Specifiers for position
Figure 27 gives the semantics of theposition specifiers. The figure writes the semantics as a vector value; the semantics of the
specifier itself is to assign the position property of the object being specified to that value. Several of the specifiers refer to
properties of self: as explained in Sec. 4, this refers to the object being constructed, and the semantics of object construction
are such that specifiers depending on other properties are only evaluated after those properties have been specified (or an
error is raised, if there are cyclic dependencies).

<!-- page:36 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
⟦at V ⟧ = ⟦V ⟧
⟦offset by V ⟧ = ⟦V relative to ego.position ⟧
⟦offset along H by V ⟧ = ⟦ego.position offset along H by V ⟧
⟦left of V ⟧ = ⟦left of V by 0 ⟧
⟦right of V ⟧ = ⟦right of V by 0 ⟧
⟦ahead of V ⟧ = ⟦ahead of V by 0 ⟧
⟦behind V ⟧ = ⟦behind V by 0 ⟧
⟦left of V by S⟧ = ⟦V ⟧ + rotate(⟨−⟦self.width⟧/2− ⟦S⟧, 0⟩ , ⟦self.heading⟧)
⟦right of V by S⟧ = ⟦V ⟧ + rotate(⟨⟦self.width⟧/2 + ⟦S⟧, 0⟩ , ⟦self.heading⟧)
⟦ahead of V by S⟧ = ⟦V ⟧ + rotate(⟨0, ⟦self.height⟧/2 + ⟦S⟧⟩ , ⟦self.heading⟧)
⟦behind V by S⟧ = ⟦V ⟧ + rotate(⟨0,−⟦self.height⟧/2− ⟦S⟧⟩ , ⟦self.heading⟧)
⟦beyond V1 by V2⟧ = ⟦beyond V1 by V2 from ego.position ⟧
⟦beyond V1 by V2 from V3⟧ = ⟦V1⟧ + rotate(⟦V2⟧, arctan(⟦V1⟧− ⟦V3⟧))
⟦visible⟧ = ⟦visible from ego ⟧
⟦visible from P⟧ = uniformPointIn(visibleRegion(P))
Figure 27. Semantics of position specifiers, given as the valuev such that the specifier evaluates to the map position7→v.

<!-- page:37 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
C.3 Specifiers for position and optionally heading
Figure 28 gives the semantics of the position specifiers that also optionally specify heading. The figure writes the semantics
as an OrientedPoint value; if this is OP , the semantics of the specifier is to assign the position property of the object being
constructed to OP.position, and the heading property of the object to OP.heading if heading is not otherwise specified
(see Sec. 4 for a discussion of optional specifiers).
⟦in R⟧ = ⟦on R⟧ =
(
OrientedPoint(x, ⟦orientation(R)⟧(x)) orientation(R) ,⊥
OrientedPoint(x,⊥) otherwise , with x = uniformPointIn(⟦R⟧)
⟦ahead of O⟧ = ⟦ahead of (front of O)⟧
⟦behind O⟧ = ⟦behind (back of O)⟧
⟦left of O⟧ = ⟦left of (left of O)⟧
⟦right of O⟧ = ⟦right of (right of O)⟧
⟦ahead of OP ⟧ = ⟦ahead of OP by 0 ⟧
⟦behind OP ⟧ = ⟦behind OP by 0 ⟧
⟦left of OP ⟧ = ⟦left of OP by 0 ⟧
⟦right of OP ⟧ = ⟦right of OP by 0 ⟧
⟦ahead of OP by S⟧ = OrientedPoint(offsetLocal(OP ,⟨0, ⟦self.height⟧/2 + ⟦S⟧⟩), ⟦OP.heading⟧)
⟦behind OP by S⟧ = OrientedPoint(offsetLocal(OP ,⟨0,−⟦self.height⟧/2− ⟦S⟧⟩), ⟦OP.heading⟧)
⟦left of OP by S⟧ = OrientedPoint(offsetLocal(OP ,⟨−⟦self.width⟧/2− ⟦S⟧, 0⟩), ⟦OP.heading⟧)
⟦right of OP by S⟧ = OrientedPoint(offsetLocal(OP ,⟨⟦self.width⟧/2 + ⟦S⟧, 0⟩), ⟦OP.heading⟧)
⟦following F for S⟧ = ⟦following F from ego.position for S⟧
⟦following F from V for S⟧ = ⟦follow F from V for S⟧
Figure 28. Semantics of position specifiers that optionally specify heading. If o is the OrientedPoint given as the semantics
above, the specifier evaluates to the map{position7→ o.position, heading7→ o.heading}.
C.4 Specifiers for heading
Figure 29 gives the semantics of the heading specifiers. As for the position specifiers above, the figure indicates the heading
value assigned by each specifier.
⟦facing H ⟧ = ⟦H ⟧
⟦facing F ⟧ = ⟦F ⟧(⟦self.position⟧)
⟦facing toward V ⟧ = arctan(⟦V ⟧− ⟦self.position⟧)
⟦facing away from V ⟧ = arctan(⟦self.position⟧− ⟦V ⟧)
⟦apparently facing H ⟧ = ⟦apparently facing H from ego.position ⟧
⟦apparently facing H from V ⟧ = ⟦H ⟧ + arctan(⟦self.position⟧− ⟦V ⟧)
Figure 29. Semantics of heading specifiers, given as the valuev such that the specifier evaluates to the map heading7→v.

<!-- page:38 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
C.5 Operators
Finally, Figures 30–35 give the semantics for Scenic’s operators, broken down by the type of value they return. We omit the
semantics for ordinary numerical and Boolean operators (max, +, or, >=, etc.), which are standard.
⟦relative heading of H ⟧ = ⟦relative heading of H from ego.heading ⟧
⟦relative heading of H1 from H2⟧ = ⟦H1⟧− ⟦H2⟧
⟦apparent heading of OP ⟧ = ⟦apparent heading of OP from ego.position ⟧
⟦apparent heading of OP from V ⟧ = ⟦OP.heading⟧− arctan(⟦OP.position⟧− ⟦V ⟧))
⟦distance to V ⟧ = ⟦distance from ego.position to V ⟧
⟦distance from V1 to V2⟧ =|⟦V2⟧− ⟦V1⟧|
⟦angle to V ⟧ = ⟦angle from ego.position to V ⟧
⟦angle from V1 to V2⟧ = arctan(⟦V2⟧− ⟦V1⟧)
Figure 30. Scalar operators.
⟦P can see O⟧ = visibleRegion(⟦P⟧)∩ boundingBox(⟦O⟧) ,∅
⟦V is in R⟧ = ⟦V ⟧∈ ⟦R⟧
⟦O is in R⟧ = boundingBox(⟦O⟧)⊆ ⟦R⟧
Figure 31. Boolean operators.
⟦F at V ⟧ = ⟦F ⟧(⟦V ⟧)
⟦F1 relative to F2⟧ = ⟦F1⟧(⟦self.position⟧) + ⟦F2⟧(⟦self.position⟧)
⟦H relative to F ⟧ = ⟦H ⟧ + ⟦F ⟧(⟦self.position⟧)
⟦F relative to H ⟧ = ⟦H ⟧ + ⟦F ⟧(⟦self.position⟧)
⟦H1 relative to H2⟧ = ⟦H1⟧ + ⟦H2⟧
Figure 32. Heading operators.
⟦V1 offset by V2⟧ = ⟦V1⟧ + ⟦V2⟧
⟦V1 offset along H by V2⟧ = ⟦V1⟧ + rotate(⟦V2⟧, ⟦H ⟧)
⟦V1 offset along F by V2⟧ = ⟦V1⟧ + rotate(⟦V2⟧, ⟦F ⟧(⟦V1⟧))
Figure 33. Vector operators.

<!-- page:39 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
⟦visible R⟧ = ⟦R visible from ego ⟧
⟦R visible from P⟧ = ⟦R⟧∩ visibleRegion(⟦P⟧)
Figure 34. Region operators.
⟦OP offset by V ⟧ = ⟦V relative to OP ⟧
⟦V relative to OP ⟧ = OrientedPoint(offsetLocal(OP , ⟦V ⟧), ⟦OP.heading⟧)
⟦follow F for S⟧ = ⟦follow F from ego.position for S⟧
⟦follow F from V for S⟧ = OrientedPoint(y, ⟦F ⟧(y)) wherey = forwardEuler(⟦V ⟧, ⟦S⟧, ⟦F ⟧)
⟦front of O⟧ = ⟦⟨0, ⟦O.height⟧/2⟩ relative to O⟧
⟦back of O⟧ = ⟦⟨0,−⟦O.height⟧/2⟩ relative to O⟧
⟦left of O⟧ = ⟦⟨−⟦O.width⟧/2, 0⟩ relative to O⟧
⟦right of O⟧ = ⟦⟨⟦O.width⟧/2, 0⟩ relative to O⟧
⟦front left of O⟧ = ⟦⟨−⟦O.width⟧/2, ⟦O.height⟧/2⟩ relative to O⟧
⟦back left of O⟧ = ⟦⟨−⟦O.width⟧/2,−⟦O.height⟧/2⟩ relative to O⟧
⟦front right of O⟧ = ⟦⟨⟦O.width⟧/2, ⟦O.height⟧/2⟩ relative to O⟧
⟦back right of O⟧ = ⟦⟨⟦O.width⟧/2,−⟦O.height⟧/2⟩ relative to O⟧
Figure 35. OrientedPoint operators.

<!-- page:40 -->
PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA Fremont, Dreossi, Ghosh, Yue, Sangiovanni-Vincentelli, and Seshia
D Additional Experiments
This section gives additional details on the experiments and
describes an experiment analogous to that of Sec. 6.3 but
using the generic two-car Scenic scenario as a baseline.
Additional Details on Experimental Setup
Since GTAV does not provide an explicit representation of
its map, we obtained an approximate map by processing a
bird’s-eye schematic view of the game world8. To identify
points on a road, we converted the image to black and white,
effectively turning roads white and everything else black.
We then used edge detection to find curbs, and computed
the nominal traffic direction by finding for each curb point
X the nearest curb point Y on the other side of the road, and
assuming traffic flows perpendicular to the segmentXY (this
was more robust than using the directions of the edges in the
image). Since the resulting road information was imperfect,
some generated scenes placed cars in undesired places such
as sidewalks or medians, and we had to manually filter the
generated images to remove these. With a real simulator, e.g.
Webots, this is not necessary.
We now define in detail the metrics used to measure the
performance of our models. Let ˆy = f(x) be the prediction
of the model f for input x. For our task, ˆy encodes bounding
boxes, scores, and categories predicted by f for the image
x. Let Bдt be a ground truth box (i.e. a bounding box from
the label of a training sample that indicates the position of
a particular object) and B ˆy be a box predicted by the model.
The Intersection over Union (IoU) is defined as IoU(Bдt , B ˆy) =
area(Bдt∩ B ˆy)/area(Bдt∪ B ˆy), where area(X) is the area of
a set X. IoU is a common evaluation metric used to measure
how well predicted bounding boxes match ground truth
boxes. We adopt the common practice of considering B ˆy a
detection for Bдt if IoU(Bдt , B ˆy) > 0.5.
Precision and recall are metrics used to measure the accu-
racy of a prediction on a particular image. Intuitively, preci-
sion is the fraction of predicted boxes that are correct, while
recall is the fraction of objects actually detected. Formally,
precision is defined astp/(tp + f p) and recall as tp/(tp + f n),
where true positives tp is the number of correct detections,
false positives f p is the number of predicted boxes that do
not match any ground truth box, and false negatives f n is
the number of ground truth boxes that are not detected. We
use average precision and recall to evaluate the performance
of a model on a collection of images constituting a test set.
Overlapping Scenario Experiments
In Sec. 6.3 we showed how we could improve the perfor-
mance of squeezeDet trained on the Driving in the Matrix
dataset [25] by replacing part of the training set with im-
ages of overlapping cars. We used the standard precision and
recall metrics defined above; however, [25] uses a different
8https://www.gtafivemap.com/
Table 9. Average precision (AP) results for the experiments
in Table 6.
Training Data Testing Data
Xmatrix / Xoverlap Tmatrix Toverlap
100% / 0% 36.1± 1.1 61 .7± 2.2
95% / 5% 36.0± 1.0 65 .8± 1.2
0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45 0.50
1
2
3
IOU
log10(number of images)
Xtwocar Xoverlap
Figure 36. Intersection Over Union (IOU) distribution for
two-car and overlapping training sets (log scale).
metric, AP (which stands for Average Precision, but is not
simply the average of the precision over the test images). For
completeness, Table 9 shows the results of our experiment
measured in AP (as computed using [4]). The outcome is the
same as before: by using the mixture, performance on over-
lapping images significantly improves, while performance
on the original dataset is unchanged.
For a cleaner comparison of overlapping vs. non-overlapping
cars, we also ran a version of the experiment in Sec. 6.3 using
the generic two-car Scenic scenario as a baseline. Specifi-
cally, we generated 1,000 images from that scenario, obtain-
ing a training set Xtwocar. We also generated 1,000 images
from the overlapping scenario to get a training set Xoverlap.
Note that Xtwocar did contain images of overlapping cars,
since the generic two-car scenario does not constrain the
cars’ locations. However, the average overlap was much
lower than that of Xoverlap, as seen in Fig. 36 (note the log
scale): thus the overlapping car images are highly “untypi-
cal” of generic two-car images. We would like to ensure the
network performs well on these difficult images by empha-
sizing them in the training set. So, as before, we constructed
various mixtures of the two training sets, fixing the total
number of images but using different ratios of images from
Xtwocar and Xoverlap. We trained the network on each of these
mixtures and evaluated their performance on 400-image test
sets Ttwocar and Toverlap from the two-car and overlapping
scenarios respectively.
To reduce the effect of randomness in training, we used
the maximum precision and recall obtained when training

<!-- page:41 -->
Scenic: A Language for Scenario Specification and Scene Generation PLDI ’19, June 22–26, 2019, Phoenix, AZ, USA
Table 10. Performance of models trained on mixtures of
Xtwocar and Xoverlap and tested on both, averaged over 8 train-
ing runs. 90/10 indicates a 9:1 mixture of Ttwocar/Toverlap.
Ttwocar Toverlap
Mixture Precision Recall Precision Recall
100/0 96.5± 1.0 95 .7± 0.5 94 .6± 1.1 82.1± 1.4
90/10 95.3± 2.1 96 .2± 0.5 93 .9± 2.5 86 .9± 1.7
80/20 96.5± 0.7 96 .0± 0.6 96 .2± 0.5 89.7± 1.4
70/30 96.5± 0.9 96 .5± 0.6 96 .0± 1.6 90 .1± 1.8
for 4,000 through 5,000 steps in increments of 250 steps. Addi-
tionally, we repeated each training 8 times, using a random
mixture each time: for example, for the 90/10 mixture of
Xtwocar and Xoverlap, each training used an independent ran-
dom choice of which 90% of Xtwocar to use and which 10% of
Xoverlap.
As Tab. 10 shows, we obtained the same results as in
Sec. 6.3: the model trained purely on generic two-car images
has high precision and recall on Ttwocar but has drastically
worse recall on Toverlap. However, devoting more of the train-
ing set to overlapping cars gives a large improvement to
recall on Toverlap while leaving performance on Ttwocar essen-
tially the same. This again demonstrates that we can improve
the performance of a network on difficult corner cases by
using Scenic to increase the representation of such cases in
the training set.
