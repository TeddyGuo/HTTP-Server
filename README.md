# HTTP Server

The HTTP server can be deployed on the MacOS or Linux.

##  Manual

> option usage:
> ==================================================================================================================================
> short option:
> ----------------------------------------------------------------------------------------------------------------------------------
>   -h : show help message
>   -c : set up the total connections can be accepted by the server. max=100,000, min=1. default=1000
>   -p : set up the port number. default=8001
>   -t : set up the worker thread number, which does not include main thread. max=16, min=1. default=4
>   -d : set up the debug level. default=1
> ==================================================================================================================================
> long option:
> ----------------------------------------------------------------------------------------------------------------------------------
> --server-type : select the server version, single or multiple thread. default=1 (single)
> ==================================================================================================================================

