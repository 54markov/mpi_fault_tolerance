/***************************************************************************************/
/* heat-2d.c: MPI implementation of Laplace equation solver by Jacobi iteration method.*/
/*                                                                                     */ 
/* 2D Laplace equation:                                                                */
/*   \Delta u = 0                                                                      */
/*   \frac{\partial^2 u}{\partial x^2} + \frac{\partial^2 u}{\partial y^2} = 0         */
/*                                                                                     */
/* Domain: x in [0, 1],  y in [0, 1]                                                   */
/* Boundary conditions:                                                                */
/*   u(x, 0) = sin(pi * x)                                                             */
/*   u(x, 1) = sin(pi * x) * exp(-pi)                                                  */
/*   u(0, y) = u(1, y) = 0                                                             */
/* Initial value for interior points is 0                                              */
/* Analytical solution:                                                                */
/*   u(x, y) = sin(pi * x) * exp(-pi * y)                                              */
/*                                                                                     */
/* Parallel implementation:                                                            */
/* 2D domain decomposition of grid [0..rows - 1][0..cols -1]                           */
/* Each process is assigned a subgrid [rows / nprocs][cols / nprocs]                   */
/*                                                                                     */
/* Input parameters: rows, cols, EPS                                                   */
/*                                                                                     */
/* Usage: mpiexec -np <p> ./heat-2d <rows> <cols>                                      */
/*                                                                                     */
/* (C) Mikhail Kurnosov, 2015                                                          */
/***************************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <mpi.h>
#include <mpi-ext.h>

#include "Utils.h"
#include "Grid.h"
#include "Task.h"

#include <algorithm>


#define EPS       0.001
#define PI        3.14159265358979323846
#define NELEMS(x) (sizeof((x)) / sizeof((x)[0]))
#define IND(i, j) ((i) * (nx + 2) + (j))

class Halo
{
    public:
        Halo(Task *task, int count,
             MPI_Datatype row, MPI_Datatype col, 
             int nx, int ny,
             MPI_Comm comm, MPI_Request* reqs)
        {
            this->task  = task;
            this->count = count;
            this->row   = row;
            this->col   = col;
            this->nx    = nx;
            this->ny    = ny;
            this->comm  = comm;
            this->reqs  = reqs;
        }

        ~Halo() { }
    
        Task* task;
        MPI_Request* reqs;
        MPI_Comm comm;
        MPI_Datatype row, col;
        int count, nx, ny;

};

static void updateInteriorPoints(double    *newGrid,
                                 double    *oldGrid,
                                 const int  ny,
                                 const int  nx)
{
    for (int i = 1; i <= ny; i++)
    {
        for (int j = 1; j <= nx; j++)
        {
            newGrid[IND(i, j)] = (oldGrid[IND(i - 1, j)] +
                                  oldGrid[IND(i + 1, j)] +
                                  oldGrid[IND(i, j - 1)] + 
                                  oldGrid[IND(i, j + 1)]) * 0.25;
        }
    }
}

static double checkTerminationCondition(double    *newGrid,
                                        double    *oldGrid,
                                        const int  ny,
                                        const int  nx)
{
    double maxdiff = 0;

    for (int i = 1; i <= ny; i++)
    {
        for (int j = 1; j <= nx; j++)
        {
            int ind = IND(i, j);
            maxdiff = fmax(maxdiff, fabs(oldGrid[ind] - newGrid[ind]));
        }
    }

    return maxdiff;
}

static void haloExchange(Halo& cookie)
{
    int i = 0;

    Task* task        = cookie.task;
    double* buf       = task->getLocalGrid();
    int count         = cookie.count;
    int nx            = cookie.nx;
    int ny            = cookie.ny;
    MPI_Datatype row  = cookie.row;
    MPI_Datatype col  = cookie.col;
    MPI_Comm     comm = cookie.comm;
    MPI_Request* reqs = cookie.reqs;

    int top   = task->getUpNeighbor();
    int down  = task->getDownNeighbor();
    int left  = task->getLeftNeighbor();
    int right = task->getRightNeighbor();

    int tagT = task->getUpTag();
    int tagD = task->getDownTag();
    int tagL = task->getLeftTag();
    int tagR = task->getRightTag();

    // TODO

    tagT = 0;
    tagD = 0;
    tagL = 0;
    tagR = 0;

    // Non-blocking recv from
    MPI_Irecv(&buf[IND(0, 1)],      count, row, top,   tagT, comm, &reqs[i++]);
    MPI_Irecv(&buf[IND(ny + 1, 1)], count, row, down,  tagD, comm, &reqs[i++]);
    MPI_Irecv(&buf[IND(1, 0)],      count, col, left,  tagL, comm, &reqs[i++]);
    MPI_Irecv(&buf[IND(1, nx + 1)], count, col, right, tagR, comm, &reqs[i++]);

    // Non-blocking send to
    MPI_Isend(&buf[IND(1, 1)],      count, row, top,   tagT, comm, &reqs[i++]);
    MPI_Isend(&buf[IND(ny, 1)],     count, row, down,  tagD, comm, &reqs[i++]);
    MPI_Isend(&buf[IND(1, 1)],      count, col, left,  tagL, comm, &reqs[i++]);
    MPI_Isend(&buf[IND(1, nx)],     count, col, right, tagR, comm, &reqs[i++]);
}

/*
static void errorHandler(MPI_Comm* pcomm, Grid* gridTask, int err)
{
    MPI_Comm comm = *pcomm;

    char errStr[MPI_MAX_ERROR_STRING] = { 0 };

    int rank   = 0;
    int size   = 0;
    int nf     = 0;
    int len    = 0;
    int eclass = 0;

    MPI_Group groupC, groupF;

    int* ranksGc = NULL;
    int* ranksGf = NULL;

    MPI_Error_class(err, &eclass);

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    MPIX_Comm_failure_ack(comm);
    MPIX_Comm_failure_get_acked(comm, &groupF);

    MPI_Group_size(groupF, &nf);
    MPI_Error_string(err, errStr, &len);

    if (MPI_ERR_PROC_FAILED != eclass)
    {
        std::cerr << "Rank " << rank
                  << " got missmatch error "
                  << errStr << std::endl;

        MPI_Abort(comm, err);
    }

    ranksGf = (int*)malloc(nf * sizeof(int));
    ranksGc = (int*)malloc(nf * sizeof(int));

    MPI_Comm_group(comm, &groupC);

    for (int i = 0; i < nf; i++)
    {
        ranksGf[i] = i;
    }

    MPI_Group_translate_ranks(groupF, nf, ranksGf, groupC, ranksGc);

    std::cout << "Rank " << rank << "/" << size - 1
            << ": Notified of error " << errStr << ". "
            << nf << " found dead: [ ";

    for (int i = 0; i < nf; i++)
    {
        std::cout << ranksGc[i] << " ";
        gridTask->kill(ranksGc[i]);
        gridTask->repair();
    }
    std::cout << "]" << std::endl;

    free(ranksGc);
    free(ranksGf);
}

static MPI_Comm repairCommunicator(MPI_Comm comm, int err)
{
    int allSucceeded = (err == MPI_SUCCESS);

    MPIX_Comm_agree(comm, &allSucceeded);
    if (!allSucceeded)
    {
        MPI_Comm commSpare;
        MPIX_Comm_revoke(comm);
        MPIX_Comm_shrink(comm, &commSpare); // Shrink the communicator
        MPI_Comm_free(&comm);               // Release the communicator
        comm = commSpare;                   // Assign shrink
    }
    else
    {
        std::cerr << "Can't repair communicator" << std::endl;
        MPI_Abort(comm, err);
    }

    return comm;
}
*/

static int solveEquation(int argc, char* argv[])
{
    int rank = 0;
    int commsize = 0;

    MPI_Init(&argc, &argv);

    double ttotal = -MPI_Wtime();

    /*
     * Create a new error handler for MPI_COMM_WORLD
     * This overrides the default MPI_ERRORS_ARE_FATAL
     * so that ranks in this communicator will not
     * automatically abort if a failure occurs.
     */
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_set_errhandler(comm, MPI_ERRORS_RETURN);

    MPI_Comm_size(comm, &commsize);
    MPI_Comm_rank(comm, &rank);

    /*
     * Create 2D grid of processes: commsize = px * py
     */
    MPI_Comm cartcomm;
    int dims[2]     = {0, 0};
    int periodic[2] = {0, 0};

    MPI_Dims_create(commsize, 2, dims);
    int px = dims[0];
    int py = dims[1];

    if (px < 2 || py < 2)
    {
        fprintf(stderr, "Invalid number of processes %d: px %d and py %d"
                        "must be greater than 1\n", commsize, px, py);
        MPI_Abort(comm, EXIT_FAILURE);
    }

    MPI_Cart_create(comm, 2, dims, periodic, 0, &cartcomm);
    int coords[2];
    MPI_Cart_coords(cartcomm, rank, 2, coords);
    int rankx = coords[0];
    int ranky = coords[1];

    int rows, cols;

    /* 
     * Broadcast command line arguments
     */
    if (rank == 0)
    {
        rows = (argc > 1) ? atoi(argv[1]) : py * 100;
        cols = (argc > 2) ? atoi(argv[2]) : px * 100;

        if (rows < py)
        {
            fprintf(stderr, "Number of rows %d less then number of py processes %d\n", rows, py);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (cols < px)
        {
            fprintf(stderr, "Number of cols %d less then number of px processes %d\n", cols, px);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        int args[2] = {rows, cols};
        MPI_Bcast(&args, NELEMS(args), MPI_INT, 0, MPI_COMM_WORLD);
    }
    else
    {
        int args[2];
        MPI_Bcast(&args, NELEMS(args), MPI_INT, 0, MPI_COMM_WORLD);
        rows = args[0];
        cols = args[1];
    }

    /* 
     * Allocate memory for local 2D subgrids with halo cells
     * [0..ny + 1][0..nx + 1]
     */
    int ny = getBlockSize(rows, ranky, py);
    int nx = getBlockSize(cols, rankx, px);

    Grid gridTask(cols, rows, nx, ny, 4, 4); // TODO

    Task* myTask = gridTask.getTask(rank);

    double* grid    = myTask->getLocalGrid();
    double* newgrid = myTask->getLocalNewGrid();

    /*
     * Fill boundary points: 
     *   - left and right borders are zero filled
     *   - top border: u(x, 0) = sin(pi * x)
     *   - bottom border: u(x, 1) = sin(pi * x) * exp(-pi)
     */
    double dx = 1.0 / (cols - 1.0); 
    int    sj = getSumOfPrevBlocks(cols, rankx, px);

    if (ranky == 0)
    {
        // Initialize top border: u(x, 0) = sin(pi * x)
        for (int j = 1; j <= nx; j++)
        {
            // Translate col index to x coord in [0, 1]
            double x     = dx * (sj + j - 1);
            int ind      = IND(0, j);
            newgrid[ind] = grid[ind] = sin(PI * x);
        }
    }

    if (ranky == py - 1)
    {
        // Initialize bottom border: u(x, 1) = sin(pi * x) * exp(-pi)
        for (int j = 1; j <= nx; j++)
        {
            // Translate col index to x coord in [0, 1]
            double x     = dx * (sj + j - 1);
            int ind      = IND(ny + 1, j);
            newgrid[ind] = grid[ind] = sin(PI * x) * exp(-PI);
        }
    }

    /*
     * Left and right borders type
     */
    MPI_Datatype col;
    MPI_Type_vector(ny, 1, nx + 2, MPI_DOUBLE, &col);
    MPI_Type_commit(&col);

    /*
     * Top and bottom borders type
     */
    MPI_Datatype row;
    MPI_Type_contiguous(nx, MPI_DOUBLE, &row);
    MPI_Type_commit(&row);

    double thalo   = 0;
    double treduce = 0;

    int niters = 0;
    double maxdiff = 0.0;
    int rc = 0;

    while (1)
    {
        niters++;

        if (niters > 1000)
        {
            std::cerr << "Overflow accepted iterations" << std::endl;
            break;
        }

        /*
         * Step 1: Update interior points
         */
        updateInteriorPoints(myTask->getLocalNewGrid(), myTask->getLocalGrid(), ny, nx);

        /*
         * Step 2: Check termination condition
         */
        maxdiff = checkTerminationCondition(myTask->getLocalNewGrid(), myTask->getLocalGrid(), ny, nx);

        /*
         * Step 3: Swap grids
         * (after termination local_grid will contain result)
         */
        myTask->swapLocalGrids();

        /*
         * Step 4: All reduce (may fail)
         */
        treduce -= MPI_Wtime();
        rc = MPI_Allreduce(MPI_IN_PLACE, &maxdiff, 1, MPI_DOUBLE, MPI_MAX, comm);
        treduce += MPI_Wtime();

        /*
         * Collective operation may fail so
         * ULFM provide repair operation
         */
        if (MPI_ERR_PROC_FAILED == rc)
        {
            
            goto exit; // TODO
        }

        if (maxdiff < EPS)
        {
            break; // Root exit point
        }

        thalo -= MPI_Wtime();

        /*
         * Step 5: Halo exchange:
         * T = 4 * (a + b * (rows / py)) + 4 * (a + b * (cols / px))
         */
        MPI_Request reqs[8];
        Halo cookie(myTask, 1, row, col, nx, ny, comm, reqs);

        haloExchange(cookie);

        /*
         * Step 6: Wait all
         */
        rc = MPI_Waitall(8, reqs, MPI_STATUS_IGNORE);
        thalo += MPI_Wtime();

        /*
         * Collective operation may fail so
         * ULFM provide repair operation
         */
        if (MPI_ERR_PROC_FAILED == rc)
        {
            // TODO
            goto exit;
        }
    }

exit:
    MPI_Type_free(&row);
    MPI_Type_free(&col);

    ttotal += MPI_Wtime();

    if (rank == 0)
    {
        printf("# Heat 2D (mpi): grid: rows %d, cols %d, procs %d (px %d, py %d)\n",
               rows, cols, commsize, px, py);
    }
    else
    {
        sleep(1); // Small delay to print
    }

    int namelen;
    char procname[MPI_MAX_PROCESSOR_NAME];

    MPI_Get_processor_name(procname, &namelen);

    printf("# P %4d (%2d, %2d) on %s: grid ny %d nx %d, total %.6f,"
           " mpi %.6f (%.2f) = allred %.6f (%.2f) + halo %.6f (%.2f)\n", 
           rank, rankx, ranky, procname, ny, nx, ttotal, treduce + thalo, 
           (treduce + thalo) / ttotal, treduce, treduce / (treduce + thalo), 
           thalo, thalo / (treduce + thalo)); 

    double prof[3] = { ttotal, treduce, thalo };

    if (rank == 0)
    {
        MPI_Reduce(MPI_IN_PLACE, prof, NELEMS(prof), MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

        sleep(1); // Small delay to print last

        printf("# procs %d : grid %d %d : niters %d : total time %.6f :"
               " mpi time %.6f : allred %.6f : halo %.6f\n", 
               commsize, rows, cols, niters, prof[0],
               prof[1] + prof[2], prof[1], prof[2]);
    }
    else
    {
        MPI_Reduce(prof, NULL, NELEMS(prof), MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}

int main(int argc, char *argv[])
{
    try
    {
        solveEquation(argc, argv);
    }
    catch (std::string err)
    {
        std::cerr << err << std::endl;
        return -1;
    }

    return 0;
}
