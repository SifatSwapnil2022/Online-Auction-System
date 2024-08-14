
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#define NUM_BIDDERS 5
#define NUM_AUCTIONS 5
#define AUCTION_DURATION 60 // 60 seconds
#define MAX_BID_COUNT_PER_BIDDER 7 // Maximum number of bids allowed per bidder
#define BIDDER_SLEEP_TIME 3 // Sleep time for the bidder threads

// Auction data structure
typedef struct {
    int start_price;
    int min_increment;
    double current_bid;
    sem_t bid_lock;
    int bid_count; // Number of bids made for this auction
} Auction;

Auction auctions[NUM_AUCTIONS];
sem_t print_lock;
pthread_mutex_t auction_close_mutex = PTHREAD_MUTEX_INITIALIZER;
int auction_closed = 0;

// Function declarations
void *bidder_thread(void *arg);
void *view_auctions(void *arg);
void *bid_auction(void *arg);
void *close_auctions(void *arg);

void initialize_auctions();
void destroy_semaphores();

int main() {
    pthread_t bidder_threads[NUM_BIDDERS];
    int bidder_ids[NUM_BIDDERS];

    printf("--------------------Welcome to the Online Auction Sytem--------------------\n");
    printf("\nPlease make a selection from the following:\n");

    // Initialize auctions
    initialize_auctions();

    sem_init(&print_lock, 0, 1);

    // Create the bidder threads
    for (int i = 0; i < NUM_BIDDERS; i++) {
        bidder_ids[i] = i + 1;
        pthread_create(&bidder_threads[i], NULL, bidder_thread, &bidder_ids[i]);
    }

    // Auction interaction loop
    char choice[20];
    while (1) {
        printf("\nView Auctions [VIEW]\nBid on an Auction [BID]\nClose Auctions [CLOSE]\nExit [EXIT]\n");
        scanf("%s", choice);

        if (strcasecmp(choice, "VIEW") == 0) {
            view_auctions(NULL);
        } else if (strcasecmp(choice, "BID") == 0) {
            bid_auction(NULL);
        } else if (strcasecmp(choice, "CLOSE") == 0) {
            close_auctions(NULL);
        } else if (strcasecmp(choice, "EXIT") == 0) {
            break;
        } else {
            printf("\nPlease choose either VIEW, BID, CLOSE, or EXIT.\n");
            continue;
        }
    }

    // Close the auction after the duration
    sleep(AUCTION_DURATION);
    pthread_mutex_lock(&auction_close_mutex);
    auction_closed = 1;
    pthread_mutex_unlock(&auction_close_mutex);

    // Wait for all bidder threads to finish
    for (int i = 0; i < NUM_BIDDERS; i++) {
        pthread_join(bidder_threads[i], NULL);
    }

    // Destroy semaphores
    destroy_semaphores();

    return 0;
}

void initialize_auctions() {
    for (int i = 0; i < NUM_AUCTIONS; i++) {
        printf("Enter the start price and minimum increment for Auction %d: ", i + 1);
        scanf("%d %d", &auctions[i].start_price, &auctions[i].min_increment);
        auctions[i].current_bid = 0;
        auctions[i].bid_count = 0; // Initialize bid count
        sem_init(&auctions[i].bid_lock, 0, 1);
    }
}

void destroy_semaphores() {
    for (int i = 0; i < NUM_AUCTIONS; i++) {
        sem_destroy(&auctions[i].bid_lock);
    }
    sem_destroy(&print_lock);
}

void *view_auctions(void *arg) {
    sem_wait(&print_lock);
    printf("\nNumber    Current Bid   Increment    Bid Count\n");
    for (int i = 0; i < NUM_AUCTIONS; i++) {
        sem_wait(&auctions[i].bid_lock);
        int min_increment = (auctions[i].current_bid == 0) ? auctions[i].start_price : auctions[i].min_increment;
        printf("%d         $%.2f         $%d         %d\n", i + 1, auctions[i].current_bid, min_increment, auctions[i].bid_count);
        sem_post(&auctions[i].bid_lock);
    }
    sem_post(&print_lock);
    return NULL;
}

void *bid_auction(void *arg) {
    int number, bid;

    printf("\nWhich auction would you like to bid on?\n");
    scanf("%d", &number);
    if (number < 1 || number > NUM_AUCTIONS) {
        printf("Invalid auction number.\n");
        return NULL;
    }

    int index = number - 1;

    sem_wait(&auctions[index].bid_lock);
    int min_bid = (auctions[index].current_bid == 0) ? auctions[index].start_price : auctions[index].current_bid + auctions[index].min_increment;
    printf("The minimum bid is $%d.\n", min_bid);
    printf("How much would you like to bid?\n");
    scanf("%d", &bid);

    if (bid < min_bid) {
        printf("Sorry, that bid is not high enough.\n");
    } else {
        auctions[index].current_bid = bid;
        auctions[index].bid_count++; // Increment bid count
    }
    sem_post(&auctions[index].bid_lock);

    return NULL;
}

void *close_auctions(void *arg) {
    pthread_mutex_lock(&auction_close_mutex);
    if (!auction_closed) {
        printf("The auction is still open. Please wait until the auction duration is over.\n");
        pthread_mutex_unlock(&auction_close_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&auction_close_mutex);

    sem_wait(&print_lock);
    for (int i = 0; i < NUM_AUCTIONS; i++) {
        sem_wait(&auctions[i].bid_lock);
        if (auctions[i].current_bid >= auctions[i].start_price) {
            printf("\nAuction %d sold for $%.2f.\n", i + 1, auctions[i].current_bid);
        } else {
            printf("\nAuction %d did not sell.\n", i + 1);
        }
        sem_post(&auctions[i].bid_lock);
    }
    sem_post(&print_lock);
    return NULL;
}

void *bidder_thread(void *arg) {
    int bidder_id = *((int *)arg);
    double bid_amount;
    int auction_index;
    int bid_count = 0;

    while (bid_count < MAX_BID_COUNT_PER_BIDDER) {
        pthread_mutex_lock(&auction_close_mutex);
        if (auction_closed) {
            pthread_mutex_unlock(&auction_close_mutex);
            break;
        }
        pthread_mutex_unlock(&auction_close_mutex);

        // Select a random auction to bid on
        auction_index = rand() % NUM_AUCTIONS;

        sem_wait(&auctions[auction_index].bid_lock);
        bid_amount = auctions[auction_index].current_bid + (rand() % 100) + 1;
        if (bid_amount > auctions[auction_index].current_bid) {
            auctions[auction_index].current_bid = bid_amount;
            printf("Bidder %d placed a bid of $%.2f on Auction %d\n", bidder_id, bid_amount, auction_index + 1);
            auctions[auction_index].bid_count++; // Increment bid count
            bid_count++;                        // Increment bidder's bid count
        }
        sem_post(&auctions[auction_index].bid_lock);

        // Sleep for a specified time before checking again
        sleep(BIDDER_SLEEP_TIME);
    }

    // After reaching maximum bid count, try to make a final bid
    if (bid_count >= MAX_BID_COUNT_PER_BIDDER) {
        double max_bid = 0;
        int max_bid_index = -1;

        for (int i = 0; i < NUM_AUCTIONS; i++) {
            sem_wait(&auctions[i].bid_lock);
            if (auctions[i].current_bid > max_bid) {
                max_bid = auctions[i].current_bid;
                max_bid_index = i;
            }
            sem_post(&auctions[i].bid_lock);
        }

        if (max_bid_index != -1) {
            sem_wait(&auctions[max_bid_index].bid_lock);
            printf("Sold out to Bidder %d for $%.2f on Auction %d!\n", bidder_id, max_bid, max_bid_index + 1);
            auction_closed = 1; // Mark the auction as closed
            sem_post(&auctions[max_bid_index].bid_lock);
        }
    }

    pthread_exit(NULL);
}


