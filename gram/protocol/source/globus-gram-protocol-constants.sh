# This file defines the GRAM protocol constants which are used by the
# C libraries and the shell script interfaces. This is converted at
# configuration time to the globus_gram_protocol_constants.h file

## globus_gram_protocol_error_t GRAM Error codes
GLOBUS_GRAM_PROTOCOL_ERROR_PARAMETER_NOT_SUPPORTED=1
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_REQUEST=2
GLOBUS_GRAM_PROTOCOL_ERROR_NO_RESOURCES=3
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_DIRECTORY=4
GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_NOT_FOUND=5
GLOBUS_GRAM_PROTOCOL_ERROR_INSUFFICIENT_FUNDS=6
GLOBUS_GRAM_PROTOCOL_ERROR_AUTHORIZATION=7
GLOBUS_GRAM_PROTOCOL_ERROR_USER_CANCELLED=8
GLOBUS_GRAM_PROTOCOL_ERROR_SYSTEM_CANCELLED=9
GLOBUS_GRAM_PROTOCOL_ERROR_PROTOCOL_FAILED=10
GLOBUS_GRAM_PROTOCOL_ERROR_STDIN_NOT_FOUND=11
GLOBUS_GRAM_PROTOCOL_ERROR_CONNECTION_FAILED=12
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAXTIME=13
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COUNT=14
GLOBUS_GRAM_PROTOCOL_ERROR_NULL_SPECIFICATION_TREE=15
GLOBUS_GRAM_PROTOCOL_ERROR_JM_FAILED_ALLOW_ATTACH=16
GLOBUS_GRAM_PROTOCOL_ERROR_JOB_EXECUTION_FAILED=17
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PARADYN=18
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBTYPE=19
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_GRAM_MYJOB=20
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_SCRIPT_ARG_FILE=21
GLOBUS_GRAM_PROTOCOL_ERROR_ARG_FILE_CREATION_FAILED=22
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOBSTATE=23
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRIPT_REPLY=24
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_SCRIPT_STATUS=25
GLOBUS_GRAM_PROTOCOL_ERROR_JOBTYPE_NOT_SUPPORTED=26
GLOBUS_GRAM_PROTOCOL_ERROR_UNIMPLEMENTED=27
GLOBUS_GRAM_PROTOCOL_ERROR_TEMP_SCRIPT_FILE_FAILED=28
GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_NOT_FOUND=29
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_USER_PROXY=30
GLOBUS_GRAM_PROTOCOL_ERROR_JOB_CANCEL_FAILED=31
GLOBUS_GRAM_PROTOCOL_ERROR_MALLOC_FAILED=32
GLOBUS_GRAM_PROTOCOL_ERROR_DUCT_INIT_FAILED=33
GLOBUS_GRAM_PROTOCOL_ERROR_DUCT_LSP_FAILED=34
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_HOST_COUNT=35
GLOBUS_GRAM_PROTOCOL_ERROR_UNSUPPORTED_PARAMETER=36
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_QUEUE=37
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_PROJECT=38
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EVALUATION_FAILED=39
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL_ENVIRONMENT=40
GLOBUS_GRAM_PROTOCOL_ERROR_DRYRUN=41
GLOBUS_GRAM_PROTOCOL_ERROR_ZERO_LENGTH_RSL=42
GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_EXECUTABLE=43
GLOBUS_GRAM_PROTOCOL_ERROR_STAGING_STDIN=44
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_MANAGER_TYPE=45
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_ARGUMENTS=46
GLOBUS_GRAM_PROTOCOL_ERROR_GATEKEEPER_MISCONFIGURED=47
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_RSL=48
GLOBUS_GRAM_PROTOCOL_ERROR_VERSION_MISMATCH=49
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ARGUMENTS=50
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_COUNT=51
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DIRECTORY=52
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_DRYRUN=53
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_ENVIRONMENT=54
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_EXECUTABLE=55
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_HOST_COUNT=56
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_JOBTYPE=57
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAXTIME=58
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MYJOB=59
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PARADYN=60
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_PROJECT=61
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_QUEUE=62
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR=63
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDIN=64
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT=65
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_JOBMANAGER_SCRIPT=66
GLOBUS_GRAM_PROTOCOL_ERROR_CREATING_PIPE=67
GLOBUS_GRAM_PROTOCOL_ERROR_FCNTL_FAILED=68
GLOBUS_GRAM_PROTOCOL_ERROR_STDOUT_FILENAME_FAILED=69
GLOBUS_GRAM_PROTOCOL_ERROR_STDERR_FILENAME_FAILED=70
GLOBUS_GRAM_PROTOCOL_ERROR_FORKING_EXECUTABLE=71
GLOBUS_GRAM_PROTOCOL_ERROR_EXECUTABLE_PERMISSIONS=72
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_STDOUT=73
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_STDERR=74
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE_USER_PROXY=75
GLOBUS_GRAM_PROTOCOL_ERROR_OPENING_CACHE=76
GLOBUS_GRAM_PROTOCOL_ERROR_INSERTING_CLIENT_CONTACT=77
GLOBUS_GRAM_PROTOCOL_ERROR_CLIENT_CONTACT_NOT_FOUND=78
GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER=79
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_CONTACT=80
GLOBUS_GRAM_PROTOCOL_ERROR_UNDEFINED_EXE=81
GLOBUS_GRAM_PROTOCOL_ERROR_CONDOR_ARCH=82
GLOBUS_GRAM_PROTOCOL_ERROR_CONDOR_OS=83
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MIN_MEMORY=84
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_MEMORY=85
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MIN_MEMORY=86
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_MEMORY=87
GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_FRAME_FAILED=88
GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_UNFRAME_FAILED=89
GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_PACK_FAILED=90
GLOBUS_GRAM_PROTOCOL_ERROR_HTTP_UNPACK_FAILED=91
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_JOB_QUERY=92
GLOBUS_GRAM_PROTOCOL_ERROR_SERVICE_NOT_FOUND=93
GLOBUS_GRAM_PROTOCOL_ERROR_JOB_QUERY_DENIAL=94
GLOBUS_GRAM_PROTOCOL_ERROR_CALLBACK_NOT_FOUND=95
GLOBUS_GRAM_PROTOCOL_ERROR_BAD_GATEKEEPER_CONTACT=96
GLOBUS_GRAM_PROTOCOL_ERROR_POE_NOT_FOUND=97
GLOBUS_GRAM_PROTOCOL_ERROR_MPIRUN_NOT_FOUND=98
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_START_TIME=99
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESERVATION_HANDLE=100
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_WALL_TIME=101
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_WALL_TIME=102
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_MAX_CPU_TIME=103
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_MAX_CPU_TIME=104
GLOBUS_GRAM_PROTOCOL_ERROR_JM_SCRIPT_NOT_FOUND=105
GLOBUS_GRAM_PROTOCOL_ERROR_JM_SCRIPT_PERMISSIONS=106
GLOBUS_GRAM_PROTOCOL_ERROR_SIGNALING_JOB=107
GLOBUS_GRAM_PROTOCOL_ERROR_UNKNOWN_SIGNAL_TYPE=108
GLOBUS_GRAM_PROTOCOL_ERROR_GETTING_JOBID=109
GLOBUS_GRAM_PROTOCOL_ERROR_WAITING_FOR_COMMIT=110
GLOBUS_GRAM_PROTOCOL_ERROR_COMMIT_TIMED_OUT=111
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SAVE_STATE=112
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_RESTART=113
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_TWO_PHASE_COMMIT=114
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_TWO_PHASE_COMMIT=115
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDOUT_POSITION=116
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDOUT_POSITION=117
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_STDERR_POSITION=118
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_STDERR_POSITION=119
GLOBUS_GRAM_PROTOCOL_ERROR_RESTART_FAILED=120
GLOBUS_GRAM_PROTOCOL_ERROR_NO_STATE_FILE=121
GLOBUS_GRAM_PROTOCOL_ERROR_READING_STATE_FILE=122
GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_STATE_FILE=123
GLOBUS_GRAM_PROTOCOL_ERROR_OLD_JM_ALIVE=124
GLOBUS_GRAM_PROTOCOL_ERROR_TTL_EXPIRED=125
GLOBUS_GRAM_PROTOCOL_ERROR_SUBMIT_UNKNOWN=126
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_REMOTE_IO_URL=127
GLOBUS_GRAM_PROTOCOL_ERROR_WRITING_REMOTE_IO_URL=128
GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE=129
GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED=130
GLOBUS_GRAM_PROTOCOL_ERROR_USER_PROXY_EXPIRED=131
GLOBUS_GRAM_PROTOCOL_ERROR_JOB_UNSUBMITTED=132
GLOBUS_GRAM_PROTOCOL_ERROR_INVALID_COMMIT=133
GLOBUS_GRAM_PROTOCOL_ERROR_RSL_SCHEDULER_SPECIFIC=134
GLOBUS_GRAM_PROTOCOL_ERROR_LAST=135

## globus_gram_protocol_job_state_t GRAM Job States
### The job is waiting for resources to become available to run.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_PENDING=1
### The job has received resources and the application is executing.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_ACTIVE=2
### The job terminated before completion because an error, user-triggered
### cancel, or system-triggered cancel.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_FAILED=4
### The job completed successfully
GLOBUS_GRAM_PROTOCOL_JOB_STATE_DONE=8
### The job has been suspended. Resources which were allocated for
### this job may have been released due to some scheduler-specific
### reason.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_SUSPENDED=16
### The job has not been submitted to the scheduler yet, pending the
### reception of the GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST
### signal from a client.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_UNSUBMITTED=32
### A mask of all job states.
GLOBUS_GRAM_PROTOCOL_JOB_STATE_ALL=0xFFFFF

## globus_gram_protocol_job_signal_t GRAM Signals
### Cancel a job
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_CANCEL=1
### Suspend a job
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_SUSPEND=2
### Resume a previously suspended job
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_RESUME=3
### Change the priority of a job
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_PRIORITY=4
### Signal the job manager to commence with a job submission if the job
### request was accompanied by the (two_state=yes) RSL attribute.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_REQUEST=5
### Signal the job manager to wait an additional number of seconds
### (specified by an integer value string as the signal's argument) before
### timing out a two-phase job commit.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_EXTEND=6
### Signal the job manager to change the way it is currently handling
### standard output and/or standard error. The argument for this
### signal is an RSL containing new @a stdout, @a stderr, @a stdout_position, 
### @a stderr_position, or @a remote_io_url relations.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_UPDATE=7
### Signal the job manager to verify  that streamed I/O has been completely
### received. The argument to this signal contains the number of bytes of stdout
### and stderr received, seperated by a space. The reply to this signal
### will be a SUCCESS message if these matched the amount sent by the
### job manager. Otherwise, an error reply indicating
### GLOBUS_GRAM_PROTOCOL_ERROR_STDIO_SIZE is returned.
### If standard output and standard error are merged, only one number should be
### sent as an argument to this signal. An argument of -1 for either stream
### size indicates that the client is not interested in the size of that
### stream.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STDIO_SIZE=8
### Signal the job manager to stop managing the current job and terminate.
### The job continues to run as normal. The job manager will send a
### state change callback with the job status being FAILED and the error
### GLOBUS_GRAM_PROTOCOL_ERROR_JM_STOPPED.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_STOP_MANAGER=9
### Signal the job manager to clean up after the completion of the job if
### the job RSL contained the (two-phase = yes) relation.
GLOBUS_GRAM_PROTOCOL_JOB_SIGNAL_COMMIT_END=10

