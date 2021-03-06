#  MPI Fault Tolerance Library
## Feature list
+ **C/C++**
+ **MPI 3.0**
+ [**User-level checkpoint library**](https://github.com/54markov/Fault-Tolerance-Library/tree/master/user-level-checkpoint "link to source files")
+ [**ULFM**](http://fault-tolerance.org/category/ulfm/ "official site ULFM") (ver 1.0 support)
* **GNU/Linux**
* **Unit test (cxxtest framework)**

## Test Samples
+ **head-2d** - [Laplace equation](https://en.wikipedia.org/wiki/Laplace%27s_equation "wiki Laplace equation") solver by [Jacobi iteration method](https://en.wikipedia.org/wiki/Jacobi_method "wiki Jacobi iteration method")
+ **n-body** - an [n-body simulation](https://en.wikipedia.org/wiki/N-body_simulation "wiki N-body simulation") approximates the motion of particles, often specifically particles that interact with one another through some type of physical forces.
+ **midpoint-rule**
+ **monte-carlo**
+ **nprimes**

## User-level checkpoint library
+ **Rollback recovery** - checkpoint/restart based 
+ **Failure	detection** - ULFM based
+ **Snapshot creation** - hard drive based (in place/via NFS)
+ **Incremental chekpointing** - delta encoding based (XOR operation)
+ **Aditional compress procedure** - [zlib](https://zlib.net/ "official site") based

## ULFM
+ **Survivability**
+ **Fault-tollerance**
+ **Compute redundancy**

## WIP
+ **Implementing alternative recovery fault tolerance methods**
+ **Expanding test sample base**
+ **Reducing overhead**
+ **Improving impementation**

## This project has been implemented as a part of my graduate thesis in Computing Systems department of Siberian State University of Telecommunications and Information Scienses.
+ **Graduate student: Vladislav Markov**
+ **Supervisor: Mikhail Kurnosov**
