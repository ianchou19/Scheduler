#include <iostream>
#include <vector>
#include <iomanip>
#include <string>
#include <algorithm>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstring>
using namespace std;


// Global variables section

int CREATED = 0;
int TRANS_TO_READY = 1;
int TRANS_TO_RUN = 2;
int TRANS_TO_BLOCK = 3;
int TRANS_TO_PREEMPT = 4;
int DONE = 5;

ifstream file;
int stat_id =0;
int used_random_index = 0; // count the index of random number which has already been used
int quantum;
int maxprios = 4;
int total_io = 0;
int last_evt_finish_time;
bool verbose = false;
bool isPreemptivePrio = false;
vector<int> random_num_vector;


// Classes section

class Process {
    public:
        int AT; int TC; int CB; int IO;
        int FT; int TT; int IT = 0; int CW = 0;
        int pid;
        int init_priority;
        int priority;
        int init_time;
        int start_time;
        int cur_cb_start_time;
        int cb_accumulation = 0;
        int cb_remaining;
        bool need_deduct_quantum = false;
        bool not_deduct_quantum_yet = true;
        
        void init(int p_pid) {
            pid = p_pid;
            priority = init_priority;
        }

        int get_AT() {return AT;}
        void set_AT(int at) {AT = at;}
        int get_TC() {return TC;}
        void set_TC(int tc) {TC = tc;}
        int get_CB() {return CB;}
        void set_CB(int cb) {CB = cb;}
        int get_IO() {return IO;}
        void set_IO(int io) {IO = io;}
        int get_TT() {return FT - get_AT();}
};

class Event {
    public:
        int event_time;
        int pid;
        int transition;
        int duration;
        int created_time;
        string last_state;

        void init(int evt_pid) {
            pid = evt_pid;
        }
};

class DES_Layer {
    public:
        std::vector<Event> DES_queue;
        bool empty = true;

        Event get_event(){
            Event event = DES_queue[0];
            DES_queue.erase(DES_queue.begin());

            if (DES_queue.empty()) {
                empty = true;
            }
            return event;
        }

        void insert_event(Event event) {
            empty = false;
            int i =0;

            while (i < DES_queue.size() && event.event_time >= DES_queue[i].event_time) {
                i++;
            }
            DES_queue.insert(DES_queue.begin() + i, event);
        }

        void delete_event(int pid) {
            for(int i = 0; i < DES_queue.size(); i++) {
                if (pid == DES_queue[i].pid) {
                    DES_queue.erase(DES_queue.begin() + i);
                }
            }
        }

        int get_next_event_time() {
            if (DES_queue.empty()) return -1;
            return DES_queue.front().event_time;
        }
};

class Scheduler {
    public:
        std::vector<Process*> run_queue;

        virtual void init(int maxprios) = 0;
        virtual Process * get_next_process() = 0;
        virtual void add_process(Process* process, bool quantum_check) = 0;
};

class SchedulerFCFS : public Scheduler {
    public:
        void init(int maxprios) {}

        void add_process(Process* process, bool quantum_check) {
            run_queue.push_back(process);
        }

        Process* get_next_process() {
            Process* process;

            if(run_queue.empty() == false) {
                process = run_queue.front();
                run_queue.erase(run_queue.begin());
            }
            else {
                return nullptr;
            }

            return process;
        }
};

class SchedulerLCFS : public Scheduler {
    public:
        void init(int maxprios) {}

        void add_process(Process* process, bool quantum_check) {
            run_queue.push_back(process);
        }

        Process* get_next_process() {
            Process* process;

            if(run_queue.empty() == false) {
                process = run_queue.back();
                run_queue.pop_back();
            }
            else {
                return nullptr;
            }

            return process;
        }
};

class SchedulerSRTF : public Scheduler {
    public:
        void init(int maxprios) {}

        void add_process(Process* process,bool quantum_check) {
            run_queue.push_back(process);
        }

        Process* get_next_process() {
            Process* process;

            if(run_queue.empty()) {
                return nullptr;
            }

            int minimum = run_queue[0] -> get_TC() - run_queue[0] -> cb_accumulation;
            int minimum_idex = 0;

            for (int i = 1; i < run_queue.size(); i++) {
                if (minimum > run_queue[i] -> get_TC() - run_queue[i] -> cb_accumulation) {
                    minimum = run_queue[i] -> get_TC() - run_queue[i] -> cb_accumulation;
                    minimum_idex = i;
                }
            }

            process = run_queue[minimum_idex];
            run_queue.erase(run_queue.begin() + minimum_idex);

            return process;
        }
};

class SchedulerRoundRobin : public Scheduler {
    public:
        void init(int maxprios) {}

        void add_process(Process* process, bool quantum_check) {
            run_queue.push_back(process);
        }

        Process* get_next_process() {
            Process* process;

            if(run_queue.empty() == false) {
                process = run_queue.front();
                run_queue.erase(run_queue.begin());
            }
            else {
                return nullptr;
            }

            return process;
        }
};

class SchedulerPriority : public Scheduler {
    public:
        vector<vector<Process*>> act;
        vector<vector<Process*>> ex;

        void init(int maxprios) {
            for (int i = 0; i < maxprios; i++) {
                vector<Process*> active_queue;
                act.push_back(active_queue);

                vector<Process*> expired_queue;
                ex.push_back(expired_queue);
            }
        }

        void add_process(Process* process, bool quantum_check) {
            int priority = process->priority;

            if (quantum_check) {
                if (priority == 0) { // in expired queue case
                    priority = process->init_priority;
                    process->priority = priority;

                    ex[priority].push_back(process);
                    return;
                }
                else { // in active queue case, decrease its priority
                    priority--;
                    process->priority = priority;
                }
            }

            act[priority].push_back(process);
        }

        Process* get_next_process() {
            if (all_queue_empty()) return nullptr;

            bool flag = true;
            for (int i = 0; i < act.size(); i++) {
                if (!act[i].empty()) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                for (int i = 0; i < act.size(); i++) {
                    act[i].swap(ex[i]);
                }
            }

            Process* process;

            for (int i = act.size() - 1; i >= 0; i--) {
                if (!act[i].empty()) {
                    process = act[i].front();
                    act[i].erase(act[i].begin());
                    break;
                }
            }

            return process;
        }

        bool all_queue_empty() {
            bool flag = true;

            for (int i = 0; i < act.size(); i++) {
                if (!act[i].empty()) {
                    flag = false;
                    break;
                }
            }

            for (int i = 0; i < ex.size(); i++) {
                if (!ex[i].empty()) {
                    flag = false;
                    break;
                }
            }

            return flag;
        }
};

class SchedulerPreemptivePriority : public Scheduler {
    public:
        vector<vector<Process*>> act;
        vector<vector<Process*>> ex;

        void init(int maxprios) {
            for (int i = 0; i < maxprios; i++) {
                vector<Process*> active_queue;
                act.push_back(active_queue);

                vector<Process*> expired_queue;
                ex.push_back(expired_queue);
            }
        }

        void add_process(Process* process, bool quantum_check) {
            int priority = process->priority;
            if (quantum_check) {
                if (priority == 0) { // in expired queue case
                    priority = process->init_priority;
                    process->priority = priority;

                    ex[priority].push_back(process);
                    return;
                }
                else { // in active queue case, decrease its priority
                    priority--;
                    process->priority = priority;
                }
            }

            act[priority].push_back(process);
        }

        Process* get_next_process() {
            if (all_queue_empty()) return nullptr;

            bool flag = true;
            for (int i = 0; i < act.size(); i++) {
                if (!act[i].empty()) {
                    flag = false;
                    break;
                }
            }

            if (flag) {
                for (int i = 0; i < act.size(); i++) {
                    act[i].swap(ex[i]);
                }
            }

            Process* process;

            for (int i = act.size() - 1; i >= 0; i--) {
                if (!act[i].empty()) {
                    process = act[i].front();
                    act[i].erase(act[i].begin());
                    break;
                }
            }

            return process;
        }

        bool all_queue_empty() {
            bool flag = true;

            for (int i = 0; i < act.size(); i++) {
                if (!act[i].empty()) {
                    flag = false;
                    break;
                }
            }

            for (int i = 0; i < ex.size(); i++) {
                if (!ex[i].empty()) {
                    flag = false;
                    break;
                }
            }

            return flag;
        }
};


// Global Instances section

DES_Layer DES; 
Scheduler* scheduler;
vector<Process> process_vector; 


// Functions section

Event createEvent(int transition, int pid, int event_time, int created_time, string last_state, int duration) {
    Event event;
    event.init(pid);
    event.transition = transition;
    event.event_time = event_time;
    event.created_time = created_time;
    event.duration = duration;
    event.last_state = last_state;

    return event;
}

int myrandom(int burst) {
    used_random_index++;

    // reset index if already been to the end of the vector
    if (used_random_index == random_num_vector.size()) used_random_index = 1;
    return 1 + (random_num_vector[used_random_index] % burst);
}

vector<Process> read_input(string input) {
    file.open(input);
    string proc_item_info;
    int pid = 0;

    while(file >> proc_item_info) {
        // create each process here
        Process process;

        process.init_priority = (myrandom(maxprios));
        process.init_priority--;

        process.init(pid);
        process.set_AT(stoi(proc_item_info)); // set arrival time

        if (!(file >> proc_item_info)) {exit(1);}
        process.set_TC(stoi(proc_item_info)); // set total CPU time

        if (!(file>>proc_item_info)) {exit(1);}
        process.set_CB(stoi(proc_item_info)); // set CPU burst

        if (!(file>>proc_item_info)) {exit(1);}
        process.set_IO(stoi(proc_item_info)); // set IO burst

        pid++;
        process_vector.push_back(process); // add process to process vector
    }

    file.close();
    return process_vector;
}

void read_rfile(string rfile) {
    file.open(rfile);
    string random_item;

    while(file >> random_item) { // read and store all the random numbers into random_num_vector vector
        random_num_vector.push_back(stoi(random_item));
    }
    file.close();
}

void PrintResult(string scheduler_type){
    cout << scheduler_type << endl; // Scheduler Info
    int final_CW = 0;
    int final_TT = 0;
    int final_TC = 0;

    for (int pid = 0; pid < process_vector.size(); pid++) {

        // get total CW/TT/TC time
        final_CW += process_vector[pid].CW;
        final_TT += process_vector[pid].get_TT();
        final_TC += process_vector[pid].get_TC();

        // per Process Info
        printf(
            "%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",
            pid, // pid
            process_vector[pid].get_AT(), process_vector[pid].get_TC(), // Arrive Time   (AT) , Total CPU Time   (TC)
            process_vector[pid].get_CB(), process_vector[pid].get_IO(), // CPU Burst   (CB) , IO Burst   (IO)
            process_vector[pid].init_priority + 1, // initial Priority assigned the process (init_priority) (only for PRIO/PREPRIO case)
            process_vector[pid].FT, process_vector[pid].get_TT(), // Finishing Time , Turnaround Time   (finishing time - AT)
            process_vector[pid].IT, process_vector[pid].CW // IO Time   (time in Blocked state) , CPU Waiting Time   (time in Ready state)
        );
    }
    // Count IO Utilization
    double io_util;
    io_util = total_io / ((double)last_evt_finish_time);

    // Count CPU Utilization
    double cpu_util;
    cpu_util = (final_TC / (double)last_evt_finish_time);

    // Summary Info
    printf(
        "SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",
        last_evt_finish_time, // Finishing Time of the last event
        (100.0 * cpu_util), (100.0 * io_util), // CPU Utilization , IO Utilization
        (final_TT / (double)process_vector.size()), // Avarage Turnaround Time among process
        (final_CW / (double)process_vector.size()), // Average CPU Waiting Time
        (process_vector.size() * 100 / (double)last_evt_finish_time) // Throughput of number process_vector per 100 time units
    );
}

void Simulation() {
    Event ev;
    Process *cur_running_proc = nullptr;
    Process *cur_running_proc2 = nullptr;
    Process *blocked_process = nullptr;
    bool call_scheduler = false;
    int cur_time;
    int burst;
    int end_of_io;
    int cur_cb_exe_time;
    int cur_cb_remain_time;
    int e_pid;
    bool last_evt_is_preempt;

    while (DES.empty != true) {
        ev = DES.get_event();
        cur_time = ev.event_time;
        e_pid = ev.pid;
        process_vector[e_pid].need_deduct_quantum = false;
        process_vector[e_pid].not_deduct_quantum_yet = true;
        last_evt_is_preempt = false;

        if (verbose) cout << cur_time << " " << e_pid ;

        // transition case: CREATED -> READY  or  BLOCK -> READY
        if (ev.transition == TRANS_TO_READY) {
            if (verbose) cout << " " << (cur_time - ev.created_time) << ": " << ev.last_state << " -> " << "READY" << endl;

            // if there's a process is currently running
            if (cur_running_proc2) {
                cur_cb_remain_time = burst - (cur_time - cur_running_proc2->cur_cb_start_time);

                // only for Preemptive Priority state
                if (isPreemptivePrio) {
                    if (cur_running_proc2->pid != ev.pid) {
                        if (verbose) cout << "---> PRIO preemption " << cur_running_proc2->pid << " by " << ev.pid
                                            << " ? TS=" << " now=" << cur_time << ")";

                        if (process_vector[ev.pid].priority > cur_running_proc2->priority 
                                && cur_cb_remain_time != 0 && cur_running_proc2->not_deduct_quantum_yet)  {
                            if (verbose) cout << "--> YES" << endl;

                            cur_cb_exe_time = cur_time - cur_running_proc2->cur_cb_start_time;

                            cur_running_proc2->cb_accumulation += cur_cb_exe_time;
                            cur_running_proc2->cb_remaining = burst - cur_cb_exe_time;

                            // deduct quantum back if the running process's cb_accumulation has been assigned 
                            // Quantum Preempt in last round
                            if (cur_running_proc2->need_deduct_quantum) {
                                // deduct qunatum or cb back depend on cases
                                if (burst > quantum) {
                                    cur_running_proc2->cb_accumulation -= quantum;
                                }
                                else {
                                    cur_running_proc2->cb_accumulation -= burst;
                                }

                                cur_running_proc2->not_deduct_quantum_yet = false;
                            }

                            Event event = createEvent(TRANS_TO_PREEMPT, cur_running_proc2->pid, 
                                            cur_time, cur_running_proc2->cur_cb_start_time, "RUNNG", cur_cb_exe_time);

                            // remove the next event for current process (remove from DES)
                            DES.delete_event(cur_running_proc2->pid);
                            DES.insert_event(event);
                        }
                        else {
                            if (verbose) cout << "--> NO" << endl;
                        }
                    }

                    last_evt_is_preempt = true; // record "the last event is preemptive type" state
                }
            }

            if (blocked_process == &process_vector[e_pid]) {
                blocked_process = nullptr;
            }

            process_vector[e_pid].init_time = cur_time;
            scheduler -> add_process(&process_vector[e_pid],false);
            call_scheduler = true;
        }

        // transition case: READY -> RUNNG
        else if(ev.transition == TRANS_TO_RUN) {
            if (verbose) cout << " " << (cur_time - process_vector[e_pid].init_time) << ": " 
                                << ev.last_state << " -> " << "RUNNG";

            cur_running_proc = &process_vector[e_pid];
            cur_running_proc2 = &process_vector[e_pid];

            // init burst depend on cases
            if (process_vector[e_pid].cb_remaining > 0) {
                burst = process_vector[e_pid].cb_remaining;
            }
            else {
                burst = myrandom(process_vector[e_pid].get_CB());
            }

            process_vector[e_pid].cur_cb_start_time = cur_time;

            if (burst >= process_vector[e_pid].get_TC() - process_vector[e_pid].cb_accumulation) {
                burst = process_vector[e_pid].get_TC() - process_vector[e_pid].cb_accumulation;
            }

            if (verbose) cout << " cb=" << burst << " rem=" << (process_vector[e_pid].get_TC() - process_vector[e_pid].cb_accumulation)
                                << " prio=" << process_vector[e_pid].priority << endl;

            process_vector[e_pid].CW += cur_time - process_vector[e_pid].init_time;

            // for Quantum Preempt case
            if (burst > quantum) {
                process_vector[e_pid].cb_accumulation += quantum;
                process_vector[e_pid].cb_remaining = burst - quantum;
                process_vector[e_pid].need_deduct_quantum = true;

                Event event = createEvent(TRANS_TO_PREEMPT, e_pid, cur_time + quantum, cur_time, "RUNNG", quantum);
                DES.insert_event(event);
            }

            // To Block case
            else if (burst + process_vector[e_pid].cb_accumulation < process_vector[e_pid].get_TC()) {
                process_vector[e_pid].cb_accumulation += burst;
                process_vector[e_pid].cb_remaining = 0;
                process_vector[e_pid].need_deduct_quantum = true;

                Event event = createEvent(TRANS_TO_BLOCK, e_pid, cur_time + burst, cur_time, "RUNNG", burst);
                DES.insert_event(event);
            }

            // for Done case
            else if (burst + process_vector[e_pid].cb_accumulation >= process_vector[e_pid].get_TC()) {
                process_vector[e_pid].cb_accumulation += burst;
                process_vector[e_pid].cb_remaining = 0;
                process_vector[e_pid].need_deduct_quantum = true;

                Event event = createEvent(DONE, e_pid, cur_time + burst, cur_time, "RUNNG", burst);
                DES.insert_event(event);
            }
        }

        // transition case: RUNNG -> READY   (Quantum runs out)
        else if(ev.transition == TRANS_TO_PREEMPT) {
            if (verbose) cout << " " << (cur_time - ev.created_time) << ": " << ev.last_state << " -> " << "READY"
                                << "  cb=" << process_vector[e_pid].cb_remaining << " rem="
                                <<process_vector[e_pid].get_TC()-process_vector[e_pid].cb_accumulation<< " prio="
                                <<process_vector[e_pid].priority<<endl;

            cur_running_proc = nullptr;
            cur_running_proc2 = nullptr;

            process_vector[e_pid].init_time = cur_time;
            scheduler -> add_process(&process_vector[e_pid], true);
            call_scheduler = true;
        }

        // transition case: RUNNG -> BLOCK
        else if(ev.transition == TRANS_TO_BLOCK) {
            if (verbose) cout << " " << (cur_time - ev.created_time) << ": " << ev.last_state << " -> " << "BLOCK";

            int IO = myrandom(process_vector[e_pid].get_IO());
            process_vector[e_pid].IT += IO;

            if (verbose) cout << "  ib=" << IO << " rem=" << (process_vector[e_pid].get_TC() - process_vector[e_pid].cb_accumulation) << endl;

            if (blocked_process == nullptr) {
                total_io += IO;
                end_of_io = IO + cur_time;
                blocked_process = &process_vector[e_pid];
            }
            else {
                if (cur_time + IO > end_of_io) {
                    total_io += cur_time + IO - end_of_io;
                    end_of_io = cur_time + IO;
                    blocked_process = &process_vector[e_pid];
                }
            }

            Event event = createEvent(TRANS_TO_READY, e_pid, cur_time + IO, cur_time, "BLOCK", IO);
            DES.insert_event(event);

            cur_running_proc = nullptr; // after Block, there's no process currently running
            cur_running_proc2 = nullptr;

            process_vector[e_pid].priority = process_vector[e_pid].init_priority;
            call_scheduler = true;
        }

        // transition case: -> DONE
        else if(ev.transition == DONE) {
            if (verbose) cout << " " << (cur_time - ev.created_time) << ": " << ev.last_state << " -> " << "DONE" <<endl;

            cur_running_proc = nullptr;
            cur_running_proc2 = nullptr;

            process_vector[e_pid].FT = cur_time;
            last_evt_finish_time = cur_time;
            call_scheduler = true;
        }

        if (call_scheduler) {
            if (DES.get_next_event_time() == cur_time) {
                continue;
            }

            call_scheduler = false;

            if (cur_running_proc == nullptr) {
                cur_running_proc = scheduler -> get_next_process();

                if (cur_running_proc == nullptr) {
                    continue;
                }

                // create event for the next process in advance
                Event event = createEvent(TRANS_TO_RUN, cur_running_proc -> pid, cur_time, cur_time, "READY", 0);
                DES.insert_event(event);
                cur_running_proc = nullptr;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    string scheduler_type; // store the Scheduler's type
    int arg_pos = 1; // determine each arguement position

    if (argv[arg_pos][1] == 'v') { // determine if is in Verbose mode
        verbose = true;
        arg_pos++;
    }

    if (argv[arg_pos][2] == 'F') { // First-Come-First-Service mode
        scheduler_type = "FCFS";
        quantum = 100000;
        scheduler = new SchedulerFCFS();
    }
    else if (argv[arg_pos][2] == 'L') { // Last-Come-First-Service mode
        scheduler_type = "LCFS";
        quantum = 100000;
        scheduler = new SchedulerLCFS();
    }
    else if (argv[arg_pos][2] == 'S') { // Shortest-Remaining-Time-First mode
        scheduler_type = "SRTF";
        quantum = 100000;
        scheduler = new SchedulerSRTF();
    }
    else if (argv[arg_pos][2] == 'R') { // Round-Robin mode
        string quantum_str;
        for (int i = 3; i < strlen(argv[arg_pos]); i++) {quantum_str += argv[arg_pos][i];}
        scheduler_type = "RR " + quantum_str;
        quantum = stoi(quantum_str);
        scheduler = new SchedulerRoundRobin();
    }
    else if (argv[arg_pos][2] == 'P') { // Priority-Scheduler mode
        string q;
        string m;
        bool has_maxpri = false; // handle the case if have specified the Max Priotiry arguement
        for (int i = 3; i < strlen(argv[arg_pos]); i++) {
            if (argv[arg_pos][i] == ':') {
                q = m;
                m = "";
                has_maxpri = true;
                continue;
            }
            m += argv[arg_pos][i];
        }

        if (has_maxpri) {
            quantum = stoi(q);
            maxprios = stoi(m);
        }
        else {
            q = m;
            quantum = stoi(q);
        }

        scheduler_type = "PRIO " + q;
        scheduler = new SchedulerPriority();
        scheduler -> init(maxprios);
    }
    else if (argv[arg_pos][2] == 'E') { // Preemptive-Priority-Scheduler mode
        string q;
        string m;
        bool has_maxpri = false; // handle the case if have specified the Max Priotiry arguement
        for (int i = 3; i < strlen(argv[arg_pos]); i++) {
            if (argv[arg_pos][i] == ':') {
                q = m;
                m = "";
                has_maxpri = true;
                continue;
            }
            m += argv[arg_pos][i];
        }

        if (has_maxpri) {
            quantum = stoi(q);
            maxprios = stoi(m);
        }
        else {
            q = m;
            quantum = stoi(q);
        }

        scheduler_type = "PREPRIO " + q;
        isPreemptivePrio = true; // specify if it is in Preemptive Priority mode
        scheduler = new SchedulerPreemptivePriority();
        scheduler -> init(maxprios);
    }

    // read the input & rfile
    arg_pos++;
    string input = argv[arg_pos];
    arg_pos++;
    string rfile = argv[arg_pos];

    read_rfile(rfile);  // read the "rfile" file  (read all the random numbers)
    read_input(input);  // read the "input" file  (create Event for each Process, put these Events into DES Layer)

    // initial the first batch of events for all the processes, put them into DES queue
    for (int i = 0; i < process_vector.size(); i++){
        Event event = createEvent(TRANS_TO_READY, i, process_vector[i].get_AT(), process_vector[i].get_AT(), "CREATED", 0);
        DES.insert_event(event);
    }

    Simulation(); // start the simulation
    PrintResult(scheduler_type); // print final output
}