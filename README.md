# CCTS-Final-Project

- here sgt2.h contains code for coarse lock implementation of SGT
- sgt3.h contains code for fine lock implementation of SGT

## In to order to compile different tests do
### fine lock
- change the header file in test.cpp to sgt3.h
- run command 'g++ -std=c++17 -pthread test.cpp -o fine_lock"
- then run ./fine_lock
### coarse lock
- change the headre file in test.cpp to sgt2.h
- run command 'g++ -std=c++17 -pthread test.cpp -o coarse_lock"
- then run ./coarse_lock
