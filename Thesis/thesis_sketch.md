# Thesis sketch

Contains the most up-to-date outline and a summary of what's being worked on.

## Outline draft

1. Introduction 

   - short introduction and a high level overview of the problem at hand (minimising programs), including the used approaches
   - should point at different chapters and summarize their content

2. Program minimisation (the order could be switched with 3)

   - in depth explanation of the problem and its goals
   - setting the domain (what kinds of programs the solution should work on)
   - explain the naive solution and its limitation (exponential complexity, potentially many useless verifications)
   - explain the solution based on delta debugging (purely delta debugging based solution) and its disadvantage of needing to run verifications during reduction
   - explain the solution based on slicing, what other approaches are used together with slicing and why they were chosen (TBD), explain why some reduction can be done correctly without needing verification

3. Automated debugging techniques

   - give a short explanation to why existing techniques are useful when reducing the size of the program

   1. Delta debugging
      - explain the idea, show the algorithm and show the link between tests in DD and verification in this work
   2. Static slicing
      - illustrate the idea, give some context and history, focus on difficulty (pointers, memory management), talk about the program dependency graph approach (PDG)
   3. Dynamic slicing
      - explain the idea and the limitations of requiring the program to run, argue whether it is a problem in our case (since user has to run the program in order to get the error info in the first case)

   - summarize the techniques and their advantages and limitations

4. C/C++ compilers and analysis tools

   - explain the requirements for the tool in which the work will be implemented (both C and C++ support, AST dumping, AST manipulation)

   1.  GCC
      - overview of GCC, explain why it was a poor choice for this project (old, complex, monolithic)
      - overview of AST related features
   2. Clang
      - overview of clang and llvm
      - overview of clanglib and why it was not used for this project
      - overview of libtooling and its advantages

   - summary of the tools and highlighting of the favourite (libtooling) 

5. Clang and LLVM
6. Implementation

7. Experiments

8. Conclusion

