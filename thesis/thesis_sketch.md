# Thesis sketch

Contains the most up-to-date outline and a summary of what's being worked on.

## Outline draft

1. Introduction 

   - short introduction and a high level overview of the problem at hand (minimising programs), including the used approaches
   - should point at different chapters and summarize their content

3. Automated debugging techniques

   - give a short explanation to why existing techniques are useful when reducing the size of the program, explain why debugging is difficult, talk about selected analysis tools

   1. Delta debugging
      - explain the idea, show the algorithm and show the link between tests in DD and verification in this work
   2. Static slicing
      - illustrate the idea, give some context and history, focus on difficulty (pointers, memory management), talk about the program dependency graph approach (PDG)
   3. Dynamic slicing
      - explain the idea and the limitations of requiring the program to run, argue whether it is a problem in our case (since user has to run the program in order to get the error info in the first case)

   - summarize the techniques and their advantages and limitations

3. C/C++ compilers and analysis tools

   - explain the requirements for the tool in which the work will be implemented (both C and C++ support, AST dumping, AST manipulation)

   1.  GCC
      - overview of GCC, explain why it was a poor choice for this project (old, complex, monolithic)
      - overview of AST related features
   2. Clang
      - overview of clang and llvm
      - overview of libclang and why it was not used for this project
      - overview of libtooling and its advantages
   3.   ANTRL
   4.  DMS

   - summary of the tools and highlighting of the favourite (libtooling) 

5. Clang libtooling

   1. Compilation databases
   2. AST
      - describe the Clang AST, how it differs from the usual AST and how its represented in memory
   3. AST visitor
      - the basic premise of the clang AST visitor and its implementation
      - explain the ability to override visit functions, returning true or false to continue
      - explain limitations (when traversing subtrees, etc.)
   4. Matchers
      - the usage, give examples
      - explain how they might be useful (fetching all functions, etc.)
   5. Source to source transformation
      - the link between the source code and the AST
      - SourceManager and Rewriter
      - TreeDiff, TreeTransform, ...
   
5. Program minimization

   - in depth explanation of the problem and its goals
   - setting the domain (what kinds of programs the solution should work on)
   - explain the naive solution and its limitation (exponential complexity, potentially many useless verifications)
   - explain the solution based on delta debugging (purely delta debugging based solution) and its disadvantage of needing to run verifications during reduction
   - explain the solution based on slicing, what other approaches are used together with slicing and why they were chosen (TBD), explain why some reduction can be done correctly without needing verification
   - explain how the verification is done naively (running a debugger and checking its output)
   - TBD: Explain how the verification is done in a more sophisticated manner

6. Implementation

   - TBD: The text depends on how the programming part goes, mention all the obstacles and challenges, mention the requirements (HW and SW, e.g. Docker), mention used work (DG, giri) and how it was used in this project

7. Experiments

   - run the project on tailored tests (i.e. programs from the target domain that are expected to "work well" with the minimization) and plot some results (reduction rate, speed, compare different approaches and highlight their strengths)
   - run the project on the SV-COMP competition benchmark and show the results

8. Conclusion

   - summarize the experiment results and the performance of the most sophisticated approach

