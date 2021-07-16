#!/usr/bin/python3

import random

# The classifications of tasks that we can have
names = ["short_task_", "med_task_", "long_task_", "io_task_", ]

# The total number of each that we've generated
count = [0, 0, 0, 0]

# the "probability" of conducting an I/O operation for each
rand_io_range = [ (10,50), (20, 60), (30, 60), (50,100)]

# approximately how long each task will take to complete
rand_time_range = [  (50, 2000), (2000, 5000),  (5000, 50000), (50, 50000)]

total_tasks = 100

with open("tasks.txt", 'w') as f:

    for _ in range(total_tasks):
        task_type = random.randrange(len(names))

        # format of each line is:
        #     task_name task_type priority task_length odds_of_IO
        f.write("%s%d %d %d %d\n"%(
            names[task_type],
            count[task_type],
            task_type,
            random.randint(*rand_time_range[task_type]),
            random.randint(*rand_io_range[task_type])
        ))

        count[task_type] += 1 # make sure each task has a unique name
