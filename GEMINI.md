We are trying to implement the Phast memory dependence predictor in gem5. The
Phast implementation is in phast.hh, phast.cc, and other files. See what source
files I changed by using `git diff stable --name-only`

A summary of Phast is in .ignore/phast-explanation.md

A deep dive of Phast is in this paper:
/home/jason/projects/cs-251a/project/Effective_Context-Sensitive_Memory_Dependence_Prediction.pdf

Think things through. Make sure you understand Phast and gem5. All code changes
must be accompanied with a justification (citing from the paper if possible).

Questions to ask yourself
- What is the purpose of this gem5 method?
- When is the method called in the instruction's lifecycle?
- How does this method implement Phast as described in the paper?
