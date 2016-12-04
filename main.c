[constant]
	MAX_FLOOR
[semaphore]
	elevator=0
	user_list
	waiting_list
	passenger_list
[shared variables]
	user_list(tid,semaphore)
	waiting_list(tid,floor,direction)
	passenger_list(tid,destination)

[control]
main(rate):
	create elevator thread;
	loop
	{
		if(time_elapsed<1s)
			wait_for(350ms);
		if(random_p>=p(time_elapsed,rate))
			create user thread;
	}
functions:
	p(time_elpased,rate)
	{
		return 1−e^−rate.time_elapsed;
	}

[elevator]
variables:
	destination
main:
	indefinite_loop
	{
		if(no_user on waiting_list)											//critical - waiting_list
			wait(elevator);
		set destination to choose_destination();
		loop(go_to_destination_by_1_floor)
		{
			for_each(passenger_leaving_at_this_floor)						//critical - passenger_list
				let_out(tid);
			for_each(user_waiting_at_this_floor)							//critical - waiting_list
				if(same_direction)
				{
					take_in(tid);
					if(this_ones_destination is farther than destination)	//critical - passenger_list
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
	let_out(tid)
	{
		kill user thread tid;
		remove from passenger_list;             //critical - passenger_list
	}
	take_in(tid)
	{
		search tid in user_list for semaphore;	//critical - user_list
		signal(that_thread_semaphore);
		remove from waiting_list;               //critical - waiting_list
	}

[user]
variables:
	direction
main(floor,deestination):
	add a semaphore to user_list;	//critical - user_list
	get direction;
	add to waiting_list;    		//critical - waiting_list
	if(only_one on user_list)		//critical - user_list
		signal(elevator);
	wait(this_thread_semaphore);
	add to passenger_list;  		//critical - passenger_list
