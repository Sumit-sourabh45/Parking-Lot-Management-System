#include <iostream>
#include <iomanip>            // std::setprecision, std::fixed 
using namespace std;

/*
 OOP Parking Lot Management System
 Data structures used:
  - vector<Slot>             : store all slots (array-like)
  - priority_queue<int,...>  : min-heaps (nearest free slot) per vehicle type
  - unordered_map<string,int>: map vehicleID -> slot index (O(1))
  - queue<WaitEntry>         : FIFO waitlist
 Billing: user supplies duration in minutes at exit (no chrono).
*/

// Vehicle types
enum class VehicleType { CAR = 0, BIKE = 1, TRUCK = 2 };

// Helper to convert enum to printable string
static string vehicleTypeToStr(VehicleType vt) {
    if (vt == VehicleType::CAR) return "CAR";
    if (vt == VehicleType::BIKE) return "BIKE";
    return "TRUCK";
}

/* ------------------ Ticket ------------------
   Simple POD representing a parking ticket.
   id       : generated ticket id (e.g. "T1")
   vehicleID: registration or unique id provided by user
   vtype    : vehicle type
   slotIndex: internal 0-based index of assigned slot
*/
class Ticket {
public:
    string id;
    string vehicleID;
    VehicleType vtype;
    int slotIndex; // internal 0-based index

    Ticket() = default;
    // Accept by const-ref for clarity and safety (no moves required)
    Ticket(const string& tid, const string& vid, VehicleType vt, int idx)
        : id(tid), vehicleID(vid), vtype(vt), slotIndex(idx) {}
};

/* ------------------ Slot ------------------
   Represents one parking slot.
   index_   : 0-based slot index
   type_    : allowed vehicle type for this slot
   occupied_: whether slot is occupied
   ticket_  : ticket info when occupied
*/
class Slot {
private:
    int index_;
    VehicleType type_;
    bool occupied_;
    Ticket ticket_; // valid only if occupied_
public:
    Slot() = default;
    Slot(int idx, VehicleType vt) : index_(idx), type_(vt), occupied_(false) {}

    int index() const { return index_; }             // 0-based internal
    VehicleType type() const { return type_; }
    bool occupied() const { return occupied_; }

    // assign a ticket and mark occupied
    void assignTicket(const Ticket& t) {
        ticket_ = t;
        occupied_ = true;
    }
    // release ticket and mark free
    Ticket releaseTicket() {
        Ticket t = ticket_;
        occupied_ = false;
        ticket_ = Ticket{};
        return t;
    }
    const Ticket& getTicket() const { return ticket_; }
};

/* ------------------ WaitEntry ------------------
   Simple struct used in the FIFO waitlist queue.
*/
struct WaitEntry {
    string vehicleID;
    VehicleType type;
    WaitEntry(string v = "", VehicleType t = VehicleType::CAR) : vehicleID(v), type(t) {}
};

/* ------------------ ParkingLot ------------------
   Encapsulates all data structures and operations:
    - slots_           : vector<Slot> (main storage)
    - freeCars_/freeBikes_/freeTrucks_ : min-heaps storing free slot indices by type
    - vehicleToSlot_   : unordered_map vehicleID -> slot index
    - waitlist_        : queue<WaitEntry> FIFO
    - rates & stats
*/
class ParkingLot {
private:
    vector<Slot> slots_;
    priority_queue<int, vector<int>, greater<int>> freeCars_;
    priority_queue<int, vector<int>, greater<int>> freeBikes_;
    priority_queue<int, vector<int>, greater<int>> freeTrucks_;
    unordered_map<string,int> vehicleToSlot_; // vehicleID -> slot index
    queue<WaitEntry> waitlist_;
    long long ticketCounter_ = 0;

    // Stats & rates
    long long totalVehiclesServed_ = 0;
    double totalEarnings_ = 0.0;
    unordered_map<VehicleType, double> ratePerHour_ {
        {VehicleType::CAR, 50.0},
        {VehicleType::BIKE, 20.0},
        {VehicleType::TRUCK, 100.0}
    };

    // Generate next ticket id
    string nextTicketID() { return "T" + to_string(++ticketCounter_); }

    // Return heap reference for a vehicle type
    priority_queue<int, vector<int>, greater<int>>& heapFor(VehicleType vt) {
        if (vt == VehicleType::CAR) return freeCars_;
        if (vt == VehicleType::BIKE) return freeBikes_;
        return freeTrucks_;
    }

public:
    ParkingLot() = default;

    // Initialize parking slots: contiguous blocks of car, bike, truck
    void initialize(int numCars, int numBikes, int numTrucks) {
        slots_.clear();
        while(!freeCars_.empty()) freeCars_.pop();
        while(!freeBikes_.empty()) freeBikes_.pop();
        while(!freeTrucks_.empty()) freeTrucks_.pop();
        vehicleToSlot_.clear();
        while(!waitlist_.empty()) waitlist_.pop();
        ticketCounter_ = 0;
        totalVehiclesServed_ = 0;
        totalEarnings_ = 0.0;

        int idx = 0;
        for(int i=0;i<numCars;++i) { slots_.emplace_back(idx, VehicleType::CAR); freeCars_.push(idx++); }
        for(int i=0;i<numBikes;++i){ slots_.emplace_back(idx, VehicleType::BIKE); freeBikes_.push(idx++); }
        for(int i=0;i<numTrucks;++i){ slots_.emplace_back(idx, VehicleType::TRUCK); freeTrucks_.push(idx++); }

        cout << "\n✅ Parking initialized: Total slots = " << slots_.size()
             << "  (Cars: " << numCars << ", Bikes: " << numBikes << ", Trucks: " << numTrucks << ")\n";
    }

    // Update hourly rate (parameter renamed to 'rate' for clarity)
    void setRate(VehicleType vt, double rate) {
        ratePerHour_[vt] = rate;
    }

    // Entry: allocate nearest free slot using min-heap; if none, add to waitlist
    void vehicleEntry(const string& vehicleID, VehicleType vt) {
        if (vehicleToSlot_.count(vehicleID)) {
            cout << "❗ Vehicle \"" << vehicleID << "\" already parked in slot " << (vehicleToSlot_[vehicleID] + 1) << "\n";
            return;
        }
        auto &heap = heapFor(vt);
        if (!heap.empty()) {
            int slotIdx = heap.top(); heap.pop();
            string tid = nextTicketID();
            Ticket t(tid, vehicleID, vt, slotIdx);
            slots_[slotIdx].assignTicket(t);
            vehicleToSlot_[vehicleID] = slotIdx;
            totalVehiclesServed_++;
            cout << "\n🎫 Ticket: " << tid << "  | Vehicle: " << vehicleID
                 << " | Type: " << vehicleTypeToStr(vt) << " | Slot#: " << (slotIdx + 1) << "\n";
        } else {
            waitlist_.emplace(vehicleID, vt);
            cout << "\n⏳ No free " << vehicleTypeToStr(vt) << " slots. Added to waitlist position " << waitlist_.size() << "\n";
        }
    }

    // Exit: user supplies duration in minutes; calculate fee; free slot; serve waitlist if applicable
    void vehicleExit(const string& vehicleID, long long durationMinutes) {
        auto it = vehicleToSlot_.find(vehicleID);
        if (it == vehicleToSlot_.end()) {
            cout << "❗ Vehicle \"" << vehicleID << "\" not found.\n";
            return;
        }
        int slotIdx = it->second;
        Slot &s = slots_[slotIdx];
        if (!s.occupied()) {
            cout << "⚠️ Internal inconsistency: slot not occupied.\n";
            vehicleToSlot_.erase(it);
            return;
        }

        if (durationMinutes < 0) durationMinutes = 0;
        // Round up minutes to hours, minimum 1 hour billed
        long long hours = (durationMinutes + 59) / 60;
        if (hours == 0) hours = 1;
        double rate = ratePerHour_[s.type()];
        double fee = hours * rate;
        totalEarnings_ += fee;

        Ticket t = s.releaseTicket();
        vehicleToSlot_.erase(it);

        cout << fixed << setprecision(2);
        cout << "\n🧾 Receipt\n"
             << "  Vehicle : " << vehicleID << "\n"
             << "  Slot    : " << (slotIdx + 1) << " (" << vehicleTypeToStr(s.type()) << ")\n"
             << "  Duration: " << durationMinutes << " minutes (" << hours << " hour(s) billed)\n"
             << "  Rate/hr : Rs " << rate << "\n"
             << "  Amount  : Rs " << fee << "\n";

        // Try to allocate the freed slot to waitlist front if it matches type
        bool assignedToWait = false;
        if (!waitlist_.empty()) {
            WaitEntry front = waitlist_.front();
            if (front.type == s.type()) {
                waitlist_.pop();
                string newTicketID = nextTicketID();
                Ticket nt(newTicketID, front.vehicleID, front.type, slotIdx);
                s.assignTicket(nt);
                vehicleToSlot_[front.vehicleID] = slotIdx;
                totalVehiclesServed_++;
                assignedToWait = true;
                cout << "➡️ Freed slot " << (slotIdx + 1) << " assigned to waitlisted vehicle \""
                     << front.vehicleID << "\" | New Ticket: " << newTicketID << "\n";
            }
        }
        if (!assignedToWait) {
            // push this slot back into free heap
            heapFor(s.type()).push(slotIdx);
        }
    }

    // Show availability & waitlist
    void displayAvailability() const {
        int freeC = (int)freeCars_.size();
        int freeB = (int)freeBikes_.size();
        int freeT = (int)freeTrucks_.size();
        cout << "\n📊 Availability: Free total = " << (freeC + freeB + freeT)
             << "  (Cars: " << freeC << ", Bikes: " << freeB << ", Trucks: " << freeT << ")\n";

        cout << "\n🚗 Occupied slots:\n";
        bool any = false;
        for (const auto &s : slots_) {
            if (s.occupied()) {
                any = true;
                const Ticket &tk = s.getTicket();
                cout << "  Slot " << (s.index()+1) << " | " << vehicleTypeToStr(s.type())
                     << " | Vehicle: " << tk.vehicleID << " | Ticket: " << tk.id << "\n";
            }
        }
        if (!any) cout << "  (none)\n";

        cout << "\n📋 Waitlist size: " << waitlist_.size() << "\n";
        if (!waitlist_.empty()) {
            queue<WaitEntry> tmp = waitlist_;
            cout << " Front -> Back:\n";
            int pos = 1;
            while (!tmp.empty()) {
                cout << "  " << pos++ << ". " << tmp.front().vehicleID << " (" << vehicleTypeToStr(tmp.front().type) << ")\n";
                tmp.pop();
            }
        }
    }

    // Show stats
    void displayStats() const {
        int occupied = 0;
        for (const auto &s : slots_) if (s.occupied()) ++occupied;
        int total = (int)slots_.size();
        double occupancy = total == 0 ? 0.0 : (100.0 * occupied / total);
        cout << fixed << setprecision(2);
        cout << "\n=== Parking Statistics ===\n";
        cout << "Total slots           : " << total << "\n";
        cout << "Currently occupied    : " << occupied << "\n";
        cout << "Occupancy percent     : " << occupancy << "%\n";
        cout << "Total served (history): " << totalVehiclesServed_ << "\n";
        cout << "Total earnings (Rs)   : " << totalEarnings_ << "\n";
        cout << "Rates per hour (Rs)   : CAR=" << ratePerHour_.at(VehicleType::CAR)
             << ", BIKE=" << ratePerHour_.at(VehicleType::BIKE)
             << ", TRUCK=" << ratePerHour_.at(VehicleType::TRUCK) << "\n";
    }

    // Print layout (1-based slot numbers for UX)
    void printSlotsLayout() const {
        cout << "\nSlots layout (Slot# : Type : Status)\n";
        for (const auto &s: slots_) {
            cout << "  " << (s.index() + 1) << " : " << vehicleTypeToStr(s.type())
                 << " : " << (s.occupied() ? ("OCC - " + s.getTicket().vehicleID) : "FREE") << "\n";
        }
    }
};

/* -------------------- Helper functions for UI -------------------- */

// parseType: map user input to VehicleType (case-insensitive)
static VehicleType parseType(const string& s) {
    string lower = s;
    for (char &c : lower) c = tolower(c);
    if (lower == "car" || lower == "c") return VehicleType::CAR;
    if (lower == "bike" || lower == "b") return VehicleType::BIKE;
    return VehicleType::TRUCK;
}

// inputPositiveInteger: robust numeric input reading for menu and minutes
static long long inputPositiveInteger(const string &prompt) {
    while (true) {
        cout << prompt;
        long long x;
        if (!(cin >> x)) {
            cin.clear();
            string bad; getline(cin, bad);
            cout << " ❗ Please enter a valid number.\n";
            continue;
        }
        if (x < 0) {
            cout << " ❗ Please enter a non-negative number.\n";
            continue;
        }
        return x;
    }
}

/* -------------------- main (user-friendly menu) -------------------- */

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ParkingLot lot;
    cout << "================ Parking Lot Management (OOP) ================\n";
    int cars = (int)inputPositiveInteger("Number of Car slots  : ");
    int bikes = (int)inputPositiveInteger("Number of Bike slots : ");
    int trucks = (int)inputPositiveInteger("Number of Truck slots: ");
    lot.initialize(cars, bikes, trucks);

    while (true) {
        cout << "\n----------------- Menu -----------------\n";
        cout << "1. Vehicle Entry\n2. Vehicle Exit (enter duration)\n3. Show Availability\n4. Show Stats\n5. Print Slots Layout\n6. Set Rate per Hour\n0. Exit\nChoose: ";
        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            string junk; getline(cin, junk);
            cout << " ❗ Invalid input.\n";
            continue;
        }

        if (choice == 0) {
            cout << "👋 Goodbye!\n";
            break;
        }

        if (choice == 1) {
            string vid, typeS;
            cout << "Enter Vehicle ID: "; cin >> vid;
            cout << "Enter Type (car/bike/truck): "; cin >> typeS;
            lot.vehicleEntry(vid, parseType(typeS));

        } else if (choice == 2) {
            string vid;
            cout << "Enter Vehicle ID to exit: "; cin >> vid;
            long long minutes = inputPositiveInteger("Enter duration in minutes (e.g. 90): ");
            lot.vehicleExit(vid, minutes);

        } else if (choice == 3) {
            lot.displayAvailability();

        } else if (choice == 4) {
            lot.displayStats();

        } else if (choice == 5) {
            lot.printSlotsLayout();

        } else if (choice == 6) {
            string ts; double rate;
            cout << "Type (car/bike/truck): "; cin >> ts;
            cout << "Rate per hour (numeric): ";
            if (!(cin >> rate) || rate < 0) {
                cin.clear(); string j; getline(cin,j);
                cout << " ❗ Invalid rate. Cancelled.\n";
            } else {
                lot.setRate(parseType(ts), rate);
                cout << "✅ Rate set.\n";
            }

        } else {
            cout << " ❗ Invalid choice. Try again.\n";
        }
    }

    return 0;
}
