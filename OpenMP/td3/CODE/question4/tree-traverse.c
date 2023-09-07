#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Structure define tree node */
typedef struct node_s
{
    int info;
    struct node_s * left;                                
    struct node_s * right;
} node_t;


/****************************************************************/

/* Description : Uses OpenMP-3.0 Task Construct.
Whenever a thread encounters a task construct, a new explicit task, An explicit
task may be executed by any thread in the current team, in parallel with other
tasks.In this approach the several task can be executed in parallel.
*/

/* @param : tree node */
/* @return the depth of the tree starting from the given node */
int inorderTraverse_Task(node_t* node, int depth) {

    if (node == NULL) {
        return depth-1;
    }

    int leftDepth, rightDepth;

    #pragma omp parallel
    #pragma omp single firstprivate(node) nowait
    #pragma omp taskgroup
    {
        #pragma omp task firstprivate(node)
        {
            leftDepth = inorderTraverse_Task(node->left, depth + 1);
        }

        #pragma omp task firstprivate(node)
        {
            rightDepth = inorderTraverse_Task(node->right, depth + 1);
        }
    }

    return (leftDepth > rightDepth) ? leftDepth : rightDepth;
}

/****************************************************************/
/* NE PAS MODIFIER CI DESSOUS */

/* Global variable declaration */
node_t * root, *p, *q;

long int totalNodes;
FILE * fp;

int max_depth_check = 0;

/* Description : Helper function to create the new node
   @param[value] : Data value of the node
   @param[depth] : depth value of the node
   */
node_t * createNewNode(int value, int depth)
{
    node_t * newnode;

    /* Allocating the memory to create the new node */
    if ((newnode = (node_t *)malloc(sizeof(node_t))) == NULL)
    {
        perror("\n\t Memory allocation for newnode ");
        exit(1);
    }

    newnode->info = value;
    newnode->right = newnode->left = NULL;
    if (depth > max_depth_check) max_depth_check = depth; 
    return (newnode);
}

/* Description : Function to create the left node of the tree
   @param [*r]: Position to insert the node
   @param[value] : data value in the node
   @param[depth] : depth value of the node

*/
void leftNode(node_t * r, int value, int depth)
{
    if (r->left != NULL)
        printf("\n Invalid !");
    else
        r->left = createNewNode(value, depth);
}

/* Description : Function to create the right node of the tree
   @param [*r]: Position to insert the node
   @param[value] : data value in the node
   @param[depth] : depth value of the node
   */
void rightNode(node_t * r, int value, int depth)
{
    if (r->right != NULL)
        printf("\n Invalid !");
    else
        r->right = createNewNode(value, depth);
}

/* Description : Function to create the Binary Search tree
   @return : Retun the pointer to the root node
   */
node_t * createTree()
{
    int index, value;

    /* Open the file to read the input data */
    if ((fp = fopen("tree-input.dat", "r")) == NULL)
    {
        printf("\n\t Failed to open the file \n");
        exit(1);
    }

    index = totalNodes;

    /* reading the first element */
    if (fscanf(fp, "%d", &value) < 0)
    {
        printf("Wrong file format : tree-input.dat file\n");
        exit(1);
    }

    /* Function call to create the first Node of the tree */
    root = createNewNode(value, 0);
    int depth;

    /* Iterate over the loop to create the tree */
    while (index > 1)
    {
        /* Reading the data from the input file */
        if (fscanf(fp, "%d", &value) < 0)
        {
            printf("Wrong file format : tree-input.dat\n");
            exit(1);
        }
        p = root;
        q = root;
        depth = 0;

        if (value == p->info)
        {
            index--;
            continue;
        }
        else
        {
            /* Check the condition for node insertion in right or left*/
            while (value != p->info && q != NULL)
            {
                p = q;
                if (value < p->info)
                {
                    q = p->left;
                }
                else
                {
                    q = p->right;
                }
                ++depth;
            }

            if (value < p->info)
            {
                /* If the value is less then the node value
                   then insert the value in left
                   */
                leftNode(p, value, depth);
            }
            else if (value > p->info)
            {
                /* If the value is greater then the node value
                   then insert the value in right
                   */
                rightNode(p, value, depth);
            }
            index--;
        }
    }

    fclose(fp);

    return root;
}

/* Description : Function to delete the Binary Search tree
 */
void deleteTree(node_t ** r)
{
    node_t *cur = *r, *temp = NULL, *parent = *r, *succ = NULL;

    if (cur == NULL)
    {
        return;
    }

    temp = *r;

    if (cur->left && cur->right)
    {
        succ = cur->right;

        while (succ->left)
        {
            parent = succ;
            succ = succ->left;
        }
        (*r)->info = succ->info;

        if (parent != *r)
        {
            parent->left = succ->right;
        }
        else
        {
            parent->right = succ->right;
        }

        temp = succ;
    }
    else
    {
        if (!cur->left)
        {
            *r = cur->right;
        }
        else if (!cur->right)
        {
            *r = cur->left;
        }
    }

    free(temp);
}

/* Main Function */
int main(int argc, char ** argv)
{
    int index = 0;
    double start_time, end_time;

    /* Checking for command line arguments */
    if (argc != 2)
    {
        printf("usage: %s [number of nodes]\n", argv[0]);
        return 1;
    }

    /* Initalizing the Number of nodes in the tree
       and the Number of threads*/
    totalNodes = atoi(argv[1]);

    if (totalNodes <= 0)
    {
        printf("\n\t Error : Number of nodes should be greater then 0\n");
        exit(1);
    }

    /* Creating the data input file */
    if ((fp = fopen("tree-input.dat", "w")) == NULL)
    {
        printf("\n\t Failed to open the file \n");
        exit(1);
    }

    srand(time(NULL));

    /* Writing the input data to the file */
    for (index = 0; index < totalNodes; index++)
    {
        fprintf(fp, "%d ", rand());
    }
    fclose(fp);

    /* Function call to create the tree */
    root = createTree();
    int depth = 0;

    start_time = omp_get_wtime();

    # pragma omp parallel
    {
        # pragma omp single
        {
            depth = inorderTraverse_Task(root, 0);  // Function call to perform the tree
        }
    }

    end_time = omp_get_wtime();

    printf("\n\t\t Total Nodes: %ld ", totalNodes);
    printf("\n\t\t Time Taken (Task Construct): %lf sec", (end_time - start_time));
    printf("\n\t\t Depth: %d (expected %d)", depth, max_depth_check);
    printf("\n\n\t Inorder tree traversal using Task Construct .................Done \n");

    while (root)
        deleteTree(&root);

    return 0;
}

