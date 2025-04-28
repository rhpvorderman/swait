/*****************************************************************************\
 *  sbatch.c - Submit a Slurm batch script.$
 *****************************************************************************
 *  Copyright (C) 2006-2007 The Regents of the University of California.
 *  Copyright (C) 2008-2010 Lawrence Livermore National Security.
 *  Copyright (C) SchedMD LLC.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Christopher J. Morrone <morrone2@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

/* This code is adapted from src/sbatch/sbatch.c */

#include <slurm/slurm.h>
#include <stdlib.h>

#define IS_JOB_FINISHED(_X)		\
	((_X->job_state & JOB_STATE_BASE) >  JOB_SUSPENDED)

#define MAX_WAIT_SLEEP_TIME 32

static int64_t _get_min_job_age() {
    slurm_conf_t *slurm_conf = NULL;
    int ret = slurm_load_ctl_conf(0, &slurm_conf);
    if (ret != SLURM_SUCCESS) {
        return SLURM_ERROR;
    }
    uint32_t min_job_age = slurm_conf->min_job_age;
    slurm_free_ctl_conf(slurm_conf);
    return min_job_age;
}

/* Wait for specified job ID to terminate, return it's exit code */
static int _job_wait(uint32_t job_id)
{
    slurm_job_info_t *job_ptr;
    job_info_msg_t *resp = NULL;
    int ec = 0, ec2, i, rc;
    int sleep_time = 2;
    bool complete = false;
    int64_t _min_job_age = _get_min_job_age();
    if (_min_job_age == SLURM_ERROR) {
        fprintf(stderr, "Unable to get minimum job age");
        return SLURM_ERROR;
    }
    uint32_t min_job_age = (uint32_t)(_min_job_age & 0xFFFFFFFF);

    while (!complete) {
        complete = true;
        sleep(sleep_time);
        /*
         * min_job_age is factored into this to ensure the job can't
         * run, complete quickly, and be purged from slurmctld before
         * we've woken up and queried the job again.
         */
        if ((sleep_time < (min_job_age / 2)) &&
            (sleep_time < MAX_WAIT_SLEEP_TIME))
            sleep_time *= 4;

        rc = slurm_load_job(&resp, job_id, SHOW_ALL);
        if (rc == SLURM_SUCCESS) {
            for (i = 0, job_ptr = resp->job_array;
                 (i < resp->record_count); i++, job_ptr++) {
                if (IS_JOB_FINISHED(job_ptr)) {
                    if (WIFEXITED(job_ptr->exit_code)) {
                        ec2 = WEXITSTATUS(job_ptr->
                                  exit_code);
                    } else
                        ec2 = 1;
                    if (ec2 > ec) {
                        ec = ec2;
                    }
                } else {
                    complete = false;
                }
            }
            slurm_free_job_info_msg(resp);
        } else if (rc == ESLURM_INVALID_JOB_ID) {
            fprintf(
                stderr, 
                "Job %u no longer found and exit code not found",
                job_id);
                return SLURM_ERROR;
        } else {
            complete = false;
            fprintf(stderr, "Currently unable to load job state information, retrying: %m");
        }
    }

    return ec;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        return -1;
    }
    slurm_init(NULL);
    char *end_ptr = NULL;
    uint32_t job_id = strtoul(argv[1], &end_ptr, 10);
    int ec = _job_wait(job_id);
    return ec;
}
