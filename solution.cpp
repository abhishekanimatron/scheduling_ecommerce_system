#include<bits/stdc++.h>
#include <thread>
using namespace std;

// Global variables for managing services, worker threads, and requests
int numberOfServices, numberOfWorkerThreads, numberOfRequests;
int waiting = 0, blocked = 0; // Counters for tracking waiting and blocked requests
vector<int> ExecutedRequest; // List of executed requests
mutex msg; // Mutex for console output synchronization
mutex waiting_lock, blocked_lock; // Mutexes for updating waiting and blocked counters
#define threshold 2 // High traffic threshold

// Structure representing a worker thread
struct workerThread {
    int tid; // Thread ID
    int priority_level; // Priority level of the thread
    int resources; // Amount of resources allocated to the thread
    bool available = 1; // Availability status of the thread
    pthread_mutex_t check_availability; // Mutex for checking thread availability
};

// Structure representing a request
struct Request {
    int request_id; // Unique ID for the request
    int transaction_type; // Type of transaction associated with the request
    int resources_required; // Resources required to process the request
    chrono::high_resolution_clock::time_point arrival_time; // Arrival time of the request
    chrono::high_resolution_clock::time_point start_time; // Start time of request processing
    chrono::high_resolution_clock::time_point completion_time; // Completion time of request processing
    chrono::milliseconds waiting_time; // Waiting time of the request
    chrono::milliseconds turnaround_time; // Turnaround time of the request
    bool isCompleted = 0; // Flag to indicate if the request has been completed
};

vector<Request*> requests; // List of all requests

// Class representing a service
class Service {
public:
    int transaction_type; // Type of transaction the service handles
    int numberOfWorkerThreads; // Number of worker threads in the service
    vector<workerThread*> wth; // List of worker threads

    // Method to initialize worker threads for the service
    void setupServiceThreads(int t_type, int nWT) {
        transaction_type = t_type; // Set transaction type
        numberOfWorkerThreads = nWT; // Set number of worker threads
        cout << "\n\n---------Enter priority level and resources assigned to each worker thread of [ Server id " << transaction_type << " ] separated by spaces--------------\n\n";
        cout << "Worker Thread [id] : [priority_level]  [resources]\n";
        int priority_level, resources;
        for(int i = 0; i < numberOfWorkerThreads; i++) {
            cout << "Worker Thread " << i << ": ";
            cin >> priority_level >> resources; // Input priority and resources
            workerThread *worker = new workerThread;
            worker->tid = i;
            worker->priority_level = priority_level;
            worker->resources = resources;
            wth.push_back(worker); // Add worker to the list
        }
        sort(wth.begin(), wth.end(), [](workerThread* a, workerThread* b) {
            return a->priority_level < b->priority_level; // Sort threads by priority
        });
    }
};

Service service[100]; // Array of services

// Function to set up services and their worker threads
void setup_services() {
    cout << "Enter number of services (n) : "; cin >> numberOfServices;
    cout << "Enter number of worker threads for each service (m) : "; cin >> numberOfWorkerThreads;

    for(int i = 0; i < numberOfServices; i++) {
        service[i].setupServiceThreads(i, numberOfWorkerThreads); // Initialize each service
    }
}

// Scheduler function to handle request allocation and processing
void* scheduler(void* id) {
    int req_id = *((int*) id);
    Request* request = requests[req_id]; // Get request object by ID
    int server_id = request->transaction_type; // Determine the server ID based on transaction type
    int resources_required = request->resources_required; // Resources needed for the request
    bool executed = false;
    bool executable = false;

    // Check if any worker thread has enough resources to handle the request
    for(int i = 0; i < service[server_id].numberOfWorkerThreads; i++) {
        workerThread* curr_worker = service[server_id].wth[i];
        if(curr_worker->resources >= resources_required) {
            executable = 1;
            break;
        }
    }

    // If no suitable worker thread is found, reject the request
    if(!executable) {
        msg.lock();
        cout << "\nRequest [" << req_id << "]  : [REJECTED]" << endl;
        msg.unlock();
        blocked_lock.lock();
        blocked++; // Increment blocked counter
        blocked_lock.unlock();
        return NULL;
    }

    waiting_lock.lock();
    waiting++; // Increment waiting counter
    if(waiting >= threshold) {
        msg.lock(); cout << "\n" << "Waiting Queue Count : " << waiting <<" [High Traffic]" << endl; msg.unlock();
    }
    waiting_lock.unlock();

    // Loop to find an available worker thread to process the request
    while(!executed) {
        for(int i = 0; i < service[server_id].numberOfWorkerThreads; i++) {
            workerThread* curr_worker = service[server_id].wth[i];
            if(curr_worker->resources < resources_required) continue;

            pthread_mutex_lock(&curr_worker->check_availability);
            if(curr_worker->available) { // Check if the worker thread is available
                curr_worker->available = 0; // Mark the worker thread as unavailable
                pthread_mutex_unlock(&curr_worker->check_availability);
                // Schedule the request for execution
                request->start_time = chrono::high_resolution_clock::now();
                usleep(5000000); // Simulate processing delay
                msg.lock();
                cout << "\nRequest [" << req_id << "] : [RUNNING] [Server " << server_id << "] [Thread " << i << "]"<< endl;
                msg.unlock();
                executed = 1; // Mark the request as executed
                request->completion_time = chrono::high_resolution_clock::now();
                request->waiting_time = chrono::duration_cast<chrono::milliseconds>(request->start_time - request->arrival_time);
                request->turnaround_time = chrono::duration_cast<chrono::milliseconds>(request->completion_time - request->arrival_time);
                pthread_mutex_lock(&curr_worker->check_availability);
                curr_worker->available = 1; // Mark the worker thread as available again
                pthread_mutex_unlock(&curr_worker->check_availability);
                break;
            } else {
                pthread_mutex_unlock(&curr_worker->check_availability);
            }
        }

        // Update status if request was successfully executed
        if(executed) {
            msg.lock();
            cout << "\nRequest [" << req_id << "] : [SUCCESS]" << endl;
            ExecutedRequest.push_back(req_id); // Add to executed requests list
            msg.unlock();
            request->isCompleted = 1;
            waiting_lock.lock();
            waiting--; // Decrement waiting counter
            waiting_lock.unlock();
        }
    }
    return NULL;
}

// Function to input and initialize requests
void input_requests() {
    cout << "\n\nEnter number of requests : "; cin >> numberOfRequests;
    cout << "\n\n--------------Enter the type of transaction(service) associated with each request and the number of resources required for that transaction(service) separated by space-------------\n\n";
    cout << "Request [id] : [transaction_type]  [resources_required]\n";
    
    int transaction_type, resources_required;
    pthread_t requestThread[numberOfRequests];
    for(int i = 0; i < numberOfRequests; i++) {
        msg.lock(); cout << "Request [" << i << "] : "; 
        cin >> transaction_type >> resources_required; msg.unlock();
        if (transaction_type < 0 || transaction_type >= numberOfServices) {
            msg.lock(); cout << "\nInvalid transaction type " << transaction_type << " Re-enter last request" << endl; msg.unlock();
            i--; // Prompt for re-entry on invalid input
            continue;
        }
        Request *req = new Request; // Create new request object
        req->request_id = i;
        req->transaction_type = transaction_type;
        req->resources_required = resources_required;
        req->arrival_time = chrono::high_resolution_clock::now(); // Record request arrival time
        requests.push_back(req); // Add to request list
    }

    // Create and join threads for each request
    for(int i = 0; i < numberOfRequests; i++) {
        requests[i]->arrival_time = chrono::high_resolution_clock::now();
        pthread_create(&requestThread[i], NULL, scheduler, (void*) &requests[i]->request_id);
    }

    for (int i = 0; i < numberOfRequests; ++i) {
        pthread_join(requestThread[i], NULL);
    }

    cout << "\n\n";

    // Display the worker thread details
    cout << left << setw(12) << "Server ID" << setw(12) << "WorkerID" << setw(12) << "Priority" << setw(12) << "Resources" << endl;
    
    for(int i = 0; i < numberOfServices ; i++) {
        for (int j = 0; j < numberOfWorkerThreads; j++) {
            cout << left << setw(12) << i << setw(12) << j << setw(12) << service[i].wth[j]->priority_level << setw(12) << service[i].wth[j]->resources << endl;
        }
    }
    cout << endl << endl;

    // Display request details including execution status
    cout << left << setw(12) << "Request ID" << setw(12) << "Server ID" << setw(15) << "Resources_Req" << setw(15) << "ArrivalTime" << setw(15) << "StartTime" << setw(18) << "CompletionTime" << setw(15) << "WaitingTime" << setw(18) << "TurnaroundTime" << endl;
    
    for(int i = 0; i < numberOfRequests; i++) {
        if(!requests[i]->isCompleted) {
            cout << left << setw(12) << i << setw(12) << requests[i]->transaction_type << setw(15) << requests[i]->resources_required << setw(15) << "-" << setw(15) << "-" << setw(18) << "-" << setw(15) << "-" << setw(18) << "-" << setw(18) << ": REJECTED" << endl;
            continue;
        }
        cout << left << setw(12) << i << setw(12) << requests[i]->transaction_type << setw(15) << requests[i]->resources_required << setw(15) << chrono::duration_cast<chrono::milliseconds>(requests[i]->arrival_time.time_since_epoch()).count() << setw(15) << chrono::duration_cast<chrono::milliseconds>(requests[i]->start_time.time_since_epoch()).count() << setw(18) << chrono::duration_cast<chrono::milliseconds>(requests[i]->completion_time.time_since_epoch()).count() << setw(15) << requests[i]->waiting_time.count() << setw(18) << requests[i]->turnaround_time.count() << endl;
    }

    // Calculate and display average waiting and turnaround times
    chrono::milliseconds average_waiting_time(0);
    chrono::milliseconds average_turnaround_time(0);
    int count = 0;
    for(Request *req: requests) {
        if(req->isCompleted) {
            average_waiting_time += req->waiting_time;
            average_turnaround_time += req->turnaround_time;
            count += 1;
        }
    }
    average_waiting_time /= count;
    average_turnaround_time /= count;
    cout << "\n\nSequence of request executed : ";
    for(auto req : ExecutedRequest) cout << req << " ";
    cout << "\nRequests rejected : " << blocked << endl;
    cout << "Average waiting time : " << average_waiting_time.count() << " ms" << endl;
    cout << "Average turnaround time :" << average_turnaround_time.count() << " ms" <<  endl;
}

// Main function to set up services and process requests
int main() {
    setup_services(); // Initialize services and worker threads
    input_requests(); // Input and handle requests
    return 0;
}
