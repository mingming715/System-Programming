/* 
 * echoserveri.c - An iterative echo server 
 */ 
#include "csapp.h"
#define NTHREADS 100
#define SBUFSIZE 100
typedef struct {
	int *buf;			/* Buffer array */
	int n;				/* Maximum number of slots */
	int front;			/* buf[(front+1)%n] is first item */
	int rear;			/* buf[rear%n] is last item */
	sem_t mutex;		/* Protects accesses to buf */
	sem_t slots;		/* Counts available slots */
	sem_t items;		/* Counts available items */
}sbuf_t;

struct item {
	int ID;			/* ID */
	int left_stock;	/* 잔여 주식 */
	int price;		/* 주식 단가 */
	int readcnt;
	sem_t mutex, w;	/* mutex lock variable */
};

typedef struct node{
	struct item data;
	struct node *left_child;
	struct node *right_child;
}node;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void insert(node **tree, int id, int left_stock, int price); /* Insertion into tree */
void delTree(node *tree); /*Deletion of binary tree */
node *search(node **tree, int id);
void print_preorder(char *result, node *tree);

node *root = NULL;
sbuf_t sbuf; /* Shared buffer of connected descriptors */
static int byte_cnt; /* Byte counter */
static sem_t mutex; /* and the mutex that protects it */

static void init_echo_cnt(void);
void echo_cnt(int connfd);

void *thread(void *vargp)
{
	Pthread_detach(pthread_self());
	while (1) {
		int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
		echo_cnt(connfd);
		Close(connfd);
	}
}
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

	int i, listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
	pthread_t tid;
	char client_hostname[MAXLINE], client_port[MAXLINE];

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);
	for(i=0; i<NTHREADS; i++)
		Pthread_create(&tid, NULL, thread, NULL);
	while (1) {
		clientlen = sizeof(struct sockaddr_storage); 
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
				client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
	}
	sbuf_deinit(&sbuf);
	delTree(root);
	exit(0);
}
/* $end echoserverimain */

static void init_echo_cnt(void)
{
	Sem_init(&mutex, 0, 1);
	byte_cnt = 0;
}

void echo_cnt(int connfd)
{
	int n;
	int id, amount;
	char buf[MAXLINE];
	rio_t rio;
	static pthread_once_t once = PTHREAD_ONCE_INIT;

	Pthread_once(&once, init_echo_cnt);
	Rio_readinitb(&rio, connfd);
	while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
		P(&mutex);
		byte_cnt += n;
		printf("thread %d received %d (%d total) bytes on fd %d\n", (int)pthread_self(), n, byte_cnt, connfd);
		char temp[MAXLINE];
		strcpy(temp, buf);
		temp[n-1]='\0';

		if(strcmp(temp, "show")==0) {
			char result[MAXLINE]="";

			print_preorder(result, root);
		
			Rio_writen(connfd, result, MAXLINE);
		}
		/*
		else if(strcmp(temp, "exit")==0) {
			printf("client on fd %d disconnected\n", connfd);
			Close(connfd);
			FD_CLR(connfd, &p->read_set);
			p->clientfd[i] = -1;
		}
		*/
		else if(strncmp(temp, "buy", 3)==0) {
			char trash[10];
			sscanf(temp, "%s %d %d", trash, &id, &amount);
			node *buy_stock = search(&root, id);	/* Find stock to buy from tree */

			if(buy_stock == NULL || amount == 0) {	/* If ID not found or amount is wrong */
				Rio_writen(connfd, "[buy] Input error\n", MAXLINE);
			}
			else {
				P(&(buy_stock->data.w));
				if(buy_stock->data.left_stock >= amount) {
					buy_stock->data.left_stock -= amount;
					Rio_writen(connfd, "[buy] success\n", MAXLINE);
				}
				else {
					Rio_writen(connfd, "Not enough left stock\n", MAXLINE);
				}
				V(&(buy_stock->data.w));
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
				P(&(sell_stock->data.w));
				sell_stock->data.left_stock += amount;
				Rio_writen(connfd, "[sell] success\n", MAXLINE);
				V(&(sell_stock->data.w));
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
		/* Update done */

		V(&mutex);
	}
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n)
{
	sp->buf = Calloc(n, sizeof(int));
	sp->n = n;						/* Buffer holds max of n items */
	sp->front = sp->rear = 0;		/* Empty buffer iff front == rear */
	Sem_init(&sp->mutex, 0, 1);		/* Binary semaphore for locking */
	Sem_init(&sp->slots, 0, n);		/* Initially, buf has n empty slots */
	Sem_init(&sp->items, 0, 0);		/* Initially, buf has 0 items */
}

/* Clean up buffer sp */
void sbuf_deinit(sbuf_t *sp)
{
	Free(sp->buf);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item)
{
	P(&sp->slots);							/* Wait for available slot */
	P(&sp->mutex);							/* Lock the buffer */
	sp->buf[(++sp->rear)%(sp->n)] = item;	/* Insert the item */
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->items);							/* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t *sp)
{
	int item;
	P(&sp->items);
	P(&sp->mutex);
	item = sp->buf[(++sp->front)%(sp->n)];	/* Remove the item */
	V(&sp->mutex);
	V(&sp->slots);
	return item;
}

void insert(node **tree, int id, int left_stock, int price){
	node *temp = NULL;
	if(!(*tree)){
		temp = (node *)malloc(sizeof(node));
		temp->left_child = temp->right_child = NULL;
		temp->data.ID = id;
		temp->data.left_stock = left_stock;
		temp->data.price = price;
		temp->data.readcnt = 0;
		Sem_init(&(temp->data.mutex), 0, 1);
		Sem_init(&(temp->data.w), 0, 1);
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

		P(&(tree->data.mutex));
		tree->data.readcnt++;
		if(tree->data.readcnt == 1)
			P(&(tree->data.w));
		V(&(tree->data.mutex));

		sprintf(tmp, "%d %d %d\n", tree->data.ID, tree->data.left_stock, tree->data.price);
		strcat(result, tmp);

		P(&(tree->data.mutex));
		tree->data.readcnt--;
		if(tree->data.readcnt == 0)
			V(&(tree->data.w));
		V(&(tree->data.mutex));

		print_preorder(result, tree->left_child);
		print_preorder(result, tree->right_child);
	}
}
