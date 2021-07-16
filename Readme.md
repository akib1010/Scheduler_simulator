# Assignment 3
* Compile the program by typing 'make'
* run the program by typing './a3q1 <Number of CPUS> <sjf/mlfq>'
* type 'make clean' to clean up
## Table for turnaround time
* The time posted on the table is the average of 5 runs  
| TaskType | Policy |  1 CPU  |  2 CPU  |  4 CPU  |  8 CPU  |  
|      0         | SJF    |   7787    |  7847    |  8085     |  7380    |
|      1         | SJF    |  64966   |  61472  |  65596   |  74804  |
|      2         | SJF    |  435914 | 409368 | 436528  | 472096 |
|      3         | SJF    |  418102 | 437329 | 478651  | 449368 |
|      0         | MLFQ| 168703  | 169251 | 162257  | 173505 |
|      1         | MLFQ| 465509  | 526970 | 503030  | 545851 |
|      2         | MLFQ| 520721  | 572497 | 452471  | 367325 |
|      3         | MLFQ| 320246  | 487179 | 538458  | 508091 |


## Table for response time
* The time posted on the table is tha average of 5 runs
| TaskType | Policy |  1 CPU  |  2 CPU  |  4 CPU  |  8 CPU  |  
|      0         | SJF    |  6797     |  6853    |  7078     |  6392    | 
|      1         | SJF    |  62587   |  58153  |  61703   |  69591  |
|      2         | SJF    | 413668  | 417857 | 406280  | 442998 |
|      3         | SJF    | 439776  | 417188 | 477982  | 471661 |
|      0         | MLFQ|  6354     |  6564    |  6306     |  5486    |
|      1         | MLFQ|  5521     |  5670    |  5488     |  4746    |
|      2         | MLFQ|  6280     |  6406    |  6153     |  5419    |
|      3         | MLFQ|  6218     |  6378    |  6131     |  5360    |

## For SJF Policy
* The turnaround time increased depending on the type of task (short to i/o), because the tasks were sorted with respect to length and the tasks with the least length had the most priority which caused turnaround time to be less for shorter tasks.  
* Similarly the response time was less for tasks with less length as those tasks had higher priority so the CPU had access to those tasks earlier.  
* Since SJF policy favors tasks with shorter length the trends in both turnaround time and response time are similar.  
* There is no significant trend in the results which indicates a correlation between the number of cpus and turnaround time or response time.  

## For MLFQ Policy
* The shorter tasks have consistent low turnaround time but the other tasks do not have any significant trends.
* The response times are very low which is expected because of the round robin fashion of the mlfq policy all the tasks get accessed by the CPU relatively faster since the arrival time of all the task is the same.
* Increasing the number of CPUs shows a correlation in response time as more CPUs means more tasks are accessed earlier resulting a lower response time if number of CPUs increases.
