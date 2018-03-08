# threadpool-busywaitproblem
The following code suffered from a busy-wait task problem using a threadpool and workers.
The problem is solved using a condition_variable which is cpu friendly.
