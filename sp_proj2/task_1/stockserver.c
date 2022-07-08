/* 
 * echoserveri.c - An iterative echo server 
 */ 
#include "csapp.h"
typedef struct {		/* Represents a pool of connected descriptors */
	int maxfd;			/* Largest descriptor in read_set */
	fd_set read_set;	/* Set of all active descriptors */
	fd_set ready_set;	/* Subset of descriptors ready for reading */
	int nready;			/* Number of ready descriptors from select */
	int maxi;			/* High water index into client array */
	int clientfd[FD_SETSIZE];	/* Set of active descriptors */
	rio_t clientrio[FD_SETSIZE];/* Set of active read buffers */
} pool;

struct item {
	int ID;			/* ID */
	int left_stock;	/* 잔여 주식 */
	int price;		/* 주식 단가 */
	sem_t mutex;	/* mutex lock variable */
};

typedef struct node{
	struct item data;
	struct node *left_child;
	struct node *right_child;
}node;

void echo(int connfd);	
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

void insert(node **tree, int id, int left_stock, int price); /* Insertion into tree */
void delTree(node *tree); /* Deletion of binary tree */
node *search(node **tree, int id);
void print_preorder(char *result, node *tree);

node *root = NULL;

/* $begin echoserverimain */
int main(int argc, char **argv) 
{
	int id, left_stock, price;
	FILE *fp = fopen("stock.txt", "r");
	if(fp==NULL) {
		printf("File does not exist.\n");
		exit(0);
	}
	while(!feof(fp)) {
		fscanf(fp, "%d %d %d", &id, &left_stock, &price);
		insert(&root, id, left_stock, price);
	}
	fclose(fp);

	int listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
	static pool pool;
	char client_hostname[MAXLINE], client_port[MAXLINE];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);
	init_pool(listenfd, &pool);
	while (1) {
		/* Wait for listening/connected descriptor(s) to become ready */
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

		/* If listening descriptor ready, add new client to pool */
		if(FD_ISSET(listenfd, &pool.ready_set)){
			clientlen = sizeof(struct sockaddr_storage); 
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
			Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
			printf("Connected to (%s, %s)\n", client_hostname, client_port);
			add_client(connfd, &pool);
		}

		/* Echo a text line from each ready connected descriptor */
		check_clients(&pool);
	}
	delTree(root);
	printf("finished\n");
	exit(0);
}
/* $end echoserverimain */


void init_pool(int listenfd, pool *p)
{
	/* Initially, there are no connected descriptors */
	int i;
	p->maxi = -1;
	for (i=0; i < FD_SETSIZE; i++)
		p->clientfd[i] = -1;

	/* Initially, listenfd is only member of select read set */
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
	int i;
	p->nready--;
	for (i = 0; i < FD_SETSIZE; i++){ /* Find an available slot */
		if (p->clientfd[i] < 0) {
			/* Add connected descriptor to the pool */
			p->clientfd[i] = connfd;
			Rio_readinitb(&p->clientrio[i], connfd);

			/* Add the descriptor to descr iptor set */
			FD_SET(connfd, &p->read_set);

			/* Update max descriptor and pool high water mark */
			if (connfd > p->maxfd)
				p->maxfd = connfd;
			if (i > p->maxi)
				p->maxi = i;
			break;
		}
	}
	if (i == FD_SETSIZE) /* Couldn't find an empty slot */
		app_error("add_client error : Too many clients");
}

void check_clients(pool *p)
{
	int i, connfd, n;
	int id, amount;
	char buf[MAXLINE];
	rio_t rio;

	for(i=0; (i <= p->maxi) && (p->nready > 0); i++) {
		connfd = p->clientfd[i];
		rio = p->clientrio[i];

		/* If the descriptor is ready, echo a text line from it */
		if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			p->nready--;
			/* Start echo */
			if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
				printf("server received %d bytes on fd %d\n", n, connfd);
				char temp[MAXLINE];
				strcpy(temp, buf);
				temp[n-1]='\0';
				
				if(strcmp(temp, "show")==0) {
					char result[MAXLINE]="";
					print_preorder(result, root);
					Rio_writen(connfd, result, MAXLINE);
				}
				else if(strcmp(temp, "exit")==0) {
					printf("client on fd %d disconnected\n", connfd);
					Close(connfd);
					FD_CLR(connfd, &p->read_set);
					p->clientfd[i] = -1;
				}
				else if(strncmp(temp, "buy", 3)==0) {
					char trash[10];
					sscanf(temp, "%s %d %d", trash, &id, &amount);
					node *buy_stock = search(&root, id);	/* Find stock to buy from tree */
					if(buy_stock == NULL || amount == 0) {	/* If ID not found or amount is wrong */
						Rio_writen(connfd, "[buy] Input error\n", MAXLINE);
					}
					else {
						if(buy_stock->data.left_stock >= amount) {
							buy_stock->data.left_stock -= amount;
							Rio_writen(connfd, "[buy] success\n", MAXLINE);
						}
						else {
							Rio_writen(connfd, "Not enough left stock\n", MAXLINE);
						}
					}
				}
				else if(strncmp(temp, "sell", 4)==0) {
					char trash[10];
					sscanf(temp, "%s %d %d", trash, &id, &amount);
					node *sell_stock = search(&root, id);
					if(sell_stock == NULL) {	/* If ID not found */
						Rio_writen(connfd, "[sell] Input error\n", MAXLINE);
					}
					else {
						sell_stock->data.left_stock += amount;
						Rio_writen(connfd, "[sell] success\n", MAXLINE);
					}
				}
				else {	
					Rio_writen(connfd, buf, MAXLINE);
				}
				/* Update stock.txt */
				char final[MAXLINE]="";
				FILE *fp2 = fopen("stock.txt", "w");
				print_preorder(final, root);
				fprintf(fp2, "%s", final);
				fclose(fp2);
			}
			/* EOF detected, remove descriptor from pool */
			else {
				Close(connfd);
				FD_CLR(connfd, &p->read_set);
				p->clientfd[i] = -1;
			}
		}
	}
}

void insert(node **tree, int id, int left_stock, int price){
	node *temp = NULL;
	if(!(*tree)){
		temp = (node *)malloc(sizeof(node));
		temp->left_child = temp->right_child = NULL;
		temp->data.ID = id;
		temp->data.left_stock = left_stock;
		temp->data.price = price;
		*tree = temp;
		return;
	}
	if(id < (*tree)->data.ID) {
		insert(&(*tree)->left_child, id, left_stock, price);
	}
	else if(id > (*tree)->data.ID) {
		insert(&(*tree)->right_child, id, left_stock, price);
	}
}

void delTree(node *tree){
	if(tree){
		delTree(tree->left_child);
		delTree(tree->right_child);
		free(tree);
	}
}

node *search(node **tree, int id){
	if(!(*tree)){
		return NULL;
	}
	if(id == (*tree)->data.ID){
		return (*tree);
	}
	else if(id < (*tree)->data.ID){
		search(&((*tree)->left_child), id);
	}
	else if(id > (*tree)->data.ID){
		search(&((*tree)->right_child), id);
	}
}

void print_preorder(char *result, node *tree){
	if(tree) {
		char tmp[MAXLINE]="";
		sprintf(tmp, "%d %d %d\n", tree->data.ID, tree->data.left_stock, tree->data.price);
		strcat(result, tmp);
		print_preorder(result, tree->left_child);
		print_preorder(result, tree->right_child);
	}
}
