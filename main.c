[shared variables]
    waiting_list(tid,floor,direction,next)
    passenger_list(tid,destination,next)

[elevator]
variables:
  destination
main:
    indefinite_loop
    {
        if(no_one on waiting_list)                      //critical - waiting_list
            sleep;
        set destination to choose_destination();
        loop(go_to_destination_by_1_floor)
        {
            for_each(passenger_leaving_at_this_floor)   //critical - passenger_list
                let_out();
            for_each(someone_waiting_at_this_floor)     //critical - waiting_list
                if(same_direction)
                {
                    take_in();
                    if(this_one's_destination is farther than destination)	//critical - passenger_list
                        overwrite destination;
                }
            wait_for_ticks(100);
        }
    }
functions:
    choose_destination()
    {
        consider farthest_floor of each side;   //critical - waiting_list
        return nearer_one;
    }
    let_out()
    {
        kill thread;
        remove from passenger_list;             //critical - passenger_list
    }
    take_in()
    {
        call thread to add itself to passenger_list;
        remove from waiting_list;               //critical - waiting_list
    }

[person]
variables:
    floor
    direction
    destination
main:
    take floor and destination as argument;
    get direction;
    add to waiting_list;
    sleep;
    add to passenger_list;
