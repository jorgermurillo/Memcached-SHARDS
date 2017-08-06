package main

import (
        "memcached/memcache"
        //"os"
        //"bufio"
        "fmt"
    	
    	"bytes"
)


func main (){

	mc := memcache.New("127.0.0.1:11211")
	fmt.Println("Connected to Memcached on IP 127.0.0.1 on Port 11211")
	var buffer bytes.Buffer

	// 35 possible keys
	var TOTAL_NUMBER_KEYS int = 35
	var object string  = "abcdefghijklmnopqrstuvwxyz123456789"

	//string_var = string_var + string("b\n")	
	fmt.Println("KEY: " + string(object[5]))

	/*To check the size of the meta data saved along with key and values in memcached, run an instance of Memcached,
	  connect to that instance through Telnet running "telnet [IP] [PORT]" (e.g telnet localhost 11211) and run this program. Once
	  the program has ended, write "stats slabs" on the telnet console. For each slab, check the chunck_size S, the # of cmd_sets 
	  done and calculate SIZE_METADATA = S - (# of cmd sets). The  value of SIZE_METADATA should be the same for each slab except the last one
	  (because it may not be full) .
	*/
    for i := 0; i < 1000; i++ {
        
        fmt.Println("Set = KEY: "+ string(object[i%TOTAL_NUMBER_KEYS]) + " VALUE: " + buffer.String())
    	mc.Set(&memcache.Item{Key: string(object[i%TOTAL_NUMBER_KEYS]), Value: buffer.Bytes()})
    	buffer.WriteString("a")
    }
	
    


}