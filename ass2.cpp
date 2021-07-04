/*
   Command line to complie: g++ -c 56200220_56198902.cpp 
   							g++ -c generator.cpp
   							g++ -o test 56200220_56198902.o generator.o -lpthread
   
   Command line to run: ./test
   Input: interval_A, interval_B, interval_C, M 
*/

#include <iostream>
#include <pthread.h>
#include <fstream>
#include <unistd.h>
#include <semaphore.h>
#include <random>


using namespace std;

#define NUM_CRAWLER 3
#define BUFFER_SIZE 12
#define BOX_SIZE 100


char* str_generator(void); //prototype of str_generator

static const int ALEN=50;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;//mutex for the buffer
static pthread_mutex_t output = PTHREAD_MUTEX_INITIALIZER;// mutex for screen output
sem_t classify;
sem_t e;//empty space in the buffer
sem_t crawl[3];
sem_t manager;

int classNum[13] = { 0 };//number of articles in each class
int cnt = 0;//count of classified articles
int key_num = 1;
int quit_craw = 0;//number of quited crawlers
bool ter = false;
int interval_A, interval_B, interval_C, M;


class MyQueue {
private:
	char arr[BUFFER_SIZE][51];
	int fro;
	int rear;
public:
	MyQueue() {
		fro = 0;
		rear = 0;
	}
	void push(char text[]) {
		if (is_available()) {
			for (int i = 0; i < 51; i++) {
				arr[rear][i] = text[i];
			}
			rear = (rear + 1) % BUFFER_SIZE;
		}
	}
	char* front()
	{
		return arr[fro];
	}
	void pop() {
		if (!is_empty()) {
			fro = (fro + 1) % BUFFER_SIZE;
		}
	}
	bool is_empty() {
		return (fro == rear);
	}
	bool is_available() {
		return !((rear + 1) % BUFFER_SIZE == fro);
	}
};

MyQueue buffer;

class MessageBox {
private:
	int box[BOX_SIZE];
	int fro;
	int rear;
public:
	MessageBox() {
		fro = 0;
		rear = 0;
	}
	void send(int n) {
		if (is_available()) {
			box[rear] = n;
			rear = (rear + 1) % BOX_SIZE;
		}
	}
	int receive()
	{
		int ret= box[fro];
		if (!is_empty()) {
			fro = (fro + 1) % BOX_SIZE;
		}
		return ret;
	}
	bool is_empty() {
		return (fro == rear);
	}
	bool is_available() {
		return !((rear + 1) % BOX_SIZE == fro);
	}
};

MessageBox mailbox;

//check terminate condition
bool ifTerminate() {
	for (int i = 0; i < 13; i++) {
		if (classNum[i] < 5) {
			return false;
		}
	}
	return true;
}

int get_length(int i) {
	int len = 0;
	while (i != 0) {
		i /= 10;
		len++;
	}
	return len;
}

void* crawler(void* args)
{

	int id = *(int*)args;
	bool iswait = false;
	bool isrest = false;
	int value;
	int count = 0;//count for grabbed articles

	//print start
	for (int i = 0; i < 13 * id + 3; i++) {
		cout << " ";
	}
	cout << "start" << endl;

	while (!ter) {

		sem_wait(&crawl[id]);

		if (isrest) {
			pthread_mutex_lock(&output);
			for (int i = 0; i < 13 * id + 2; i++) { cout << " "; } cout << "s-rest" << endl;
			pthread_mutex_unlock(&output);
			isrest = false;
		}

		sem_wait(&e);//empty space left, blocked if zero

		if (iswait)
		{
			pthread_mutex_lock(&output);
			for (int i = 0; i < 13 * id + 2; i++) { cout << " "; } cout << "s-wait" << endl;
			pthread_mutex_unlock(&output);
			iswait = false;
		}

		if (ter) {
			break;
		}

		pthread_mutex_lock(&output);
		for (int i = 0; i < 13 * id + 4; i++) { cout << " "; } cout << "grab" << endl;
		pthread_mutex_unlock(&output);

		usleep(interval_A); // simulate time interval A

		char* grab = str_generator();
		count++;


		pthread_mutex_lock(&mtx);
		buffer.push(grab);
		pthread_mutex_unlock(&mtx);

		pthread_mutex_lock(&output);
		for (int i = 0; i < 13 * id + 2; i++) { cout << " "; } cout << "f-grab" << endl;
		pthread_mutex_unlock(&output);

		//check the number grabbed article
		if (count < M) {
			sem_post(&crawl[id]);
		}
		else if (count == M) {
			count = 0;
			mailbox.send(id);
			sem_post(&manager);
			isrest = true;

			pthread_mutex_lock(&output);
			for (int i = 0; i < 13 * id + 4; i++) { cout << " "; } cout << "rest" << endl;
			pthread_mutex_unlock(&output);
		}

		//check if the buffer is full
		sem_getvalue(&e, &value);
		if (value == 0) {
			iswait = true;
			pthread_mutex_lock(&output);
			for (int i = 0; i < 13 * id + 4; i++) { cout << " "; } cout << "wait" << endl;
			pthread_mutex_unlock(&output);
		}

		sem_post(&classify);

	}

	pthread_mutex_lock(&output);
	for (int i = 0; i < 13 * id + 4; i++) { cout << " "; } cout << "quit" << endl;
	pthread_mutex_unlock(&output);

	quit_craw++;//increase the number of quited crawlers

	if (quit_craw == NUM_CRAWLER) {//if all crawlers quited, signal manager
		sem_post(&manager);
	}

	pthread_exit(NULL);
}

void* classifier(void* args) {

	pthread_mutex_lock(&output);
	for (int i = 0; i < 43; i++) { cout << " "; } cout << "start" << endl;
	pthread_mutex_unlock(&output);

	ofstream fout;
	fout.open("corpus.txt");
	int len;
	char copy[51];
	bool first_ter = true;//if the terminate condition is met for the first time



	while (true) {

		sem_wait(&classify);

		if (ifTerminate()) {
			ter = true;
			if (first_ter) {
				len = get_length(cnt);

				pthread_mutex_lock(&output);
				for (int i = 0; i < 41 - len; i++) { cout << " "; } cout << cnt << "-enough" << endl;
				pthread_mutex_unlock(&output);

				first_ter = false;
			}
			else if (buffer.is_empty()) {
				len = get_length(cnt);

				pthread_mutex_lock(&output);
				for (int i = 0; i < 42 - len; i++) { cout << " "; } cout << cnt << "-store" << endl;
				pthread_mutex_unlock(&output);

				break;
			}

		}

		pthread_mutex_lock(&output);
		for (int i = 0; i < 44; i++) { cout << " "; } cout << "clfy" << endl;
		pthread_mutex_unlock(&output);

		//copy		
		char* c = buffer.front();
		int j = 0;
		for (int i = 0; i < 50; i++) {
			if (c[i] >= 'a' && c[i] <= 'z') {
				copy[j] = c[i];
				j++;
			}
			else if (c[i] >= 'A' && c[i] <= 'Z') {
				copy[j] = c[i] + 32;
				j++;
			}
		}

		//classification
		int label = int(copy[0] - 'a') % 13 + 1;
		classNum[label - 1]++;

		//output in txt
		fout << key_num << " " << label << " " << buffer.front() << endl;

		key_num++;
		cnt++;

		usleep(interval_B);//simulate time interval B

		//delete
		pthread_mutex_lock(&mtx);
		buffer.pop();
		pthread_mutex_unlock(&mtx);

		pthread_mutex_lock(&output);
		for (int i = 0; i < 42; i++) { cout << " "; } cout << "f-clfy" << endl;
		pthread_mutex_unlock(&output);

		sem_post(&e);

		//if all crawlers are therminated, signal classifier
		if (quit_craw == NUM_CRAWLER) {
			sem_post(&classify);
		}

	}
	fout.close();

	pthread_mutex_lock(&output);
	for (int i = 0; i < 44; i++) { cout << " "; } cout << "quit" << endl;
	pthread_mutex_unlock(&output);

	pthread_exit(NULL);
}

void* strategy_manager(void* args)
{
	int id;

	pthread_mutex_lock(&output);
	for (int i = 0; i < 64; i++) {cout << " ";} cout << "start" << endl;
	pthread_mutex_unlock(&output);
	

	while (!ter) {

		sem_wait(&manager);

		id = mailbox.receive();//receive the crawler id
		pthread_mutex_lock(&output);
		for (int i = 0; i < 62; i++) {cout << " ";} cout << "get-crx" << endl;
		pthread_mutex_unlock(&output);

		usleep(interval_C);//simulate time interval C

		pthread_mutex_lock(&output);
		for (int i = 0; i < 63; i++) {cout << " ";}cout << "up-crx" << endl;
		pthread_mutex_unlock(&output);

		sem_post(&crawl[id]);
	}

	pthread_mutex_lock(&output);
	for (int i = 0; i < 65; i++) {cout << " "; } cout << "quit" << endl;
	pthread_mutex_unlock(&output);

	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
	pthread_t threadcr[NUM_CRAWLER], threadcl, threadm;
	int threadid[NUM_CRAWLER];
	int i, rc;

	sem_init(&classify, 0, 0);//initially block the classifier
	sem_init(&e, 0, BUFFER_SIZE);//initial buffer size
	sem_init(&crawl[0], 0, 1);//initially unblocked
	sem_init(&crawl[1], 0, 1);
	sem_init(&crawl[2], 0, 1);
	sem_init(&manager, 0, 0);//initially block the manager

	cin >> interval_A >> interval_B >> interval_C >> M;
	cout << "crawler1     crawler2     crawler3     classfier     strategy-manager" << endl;

	for (i = 0; i < NUM_CRAWLER; i++) {
		threadid[i] = i;
		rc = pthread_create(&threadcr[i], NULL, crawler, (void*)&threadid[i]);
		if (rc) {
			cout << "Error when creating thread!" << endl;
			exit(-1);
		}
	}

	rc = pthread_create(&threadcl, NULL, classifier, NULL);
	if (rc) {
		cout << "Error when creating threads!" << endl;
		exit(-1);
	}

	rc = pthread_create(&threadm, NULL, strategy_manager, NULL);
	if (rc) {
		cout << "Error when creating threads!" << endl;
		exit(-1);
	}

	for (i = 0; i < NUM_CRAWLER; i++) {
		rc = pthread_join(threadcr[i], NULL);
		if (rc) {
			cout << "Error when joining thread!" << endl;
			exit(-1);
		}

	}

	rc = pthread_join(threadcl, NULL);
	if (rc) {
		cout << "Error when joining threads!" << endl;
		exit(-1);
	}

	rc = pthread_join(threadm, NULL);
	if (rc) {
		cout << "Error when joining threads!" << endl;
		exit(-1);
	}

	sem_destroy(&classify);
	sem_destroy(&e);
	sem_destroy(&crawl[0]);
	sem_destroy(&crawl[1]);
	sem_destroy(&crawl[2]);
	sem_destroy(&manager);

	pthread_exit(NULL);
}
