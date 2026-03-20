We are trying to implement the Phast memory dependence predictor in gem5. The
Phast implementation is in phast.hh, phast.cc, and other files. See what source
files I changed by using `git diff main --name-only`

Phast memory dependence predictor is described in this paper:
.ignore/Effective_Context-Sensitive_Memory_Dependence_Prediction-1.pdf

Think things through. Make sure you understand Phast and gem5. All code changes
must be accompanied with a justification (citing from the paper if possible).

Questions to ask yourself
- What is the purpose of the gem5 method you want to change?
- When is the method called in the instruction's lifecycle?
- How would your change implement Phast as described in the paper?
