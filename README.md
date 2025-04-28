# swait
Slurm command that waits on job exit and returns job exit code.

This is simply reusing code from [src/sbatch/sbatch.c](
https://github.com/SchedMD/slurm/blob/master/src/sbatch/sbatch.c) from the SLURM
project.

# Usage
```
swait <job_id>
```

# Compilation

```
cc -o swait swait.c  -lslurm
```
