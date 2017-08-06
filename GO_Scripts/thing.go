package main

import (
        "memcached/memcache"
        "os"
        "bufio"
        "fmt"
    	"log"
)

func main() {

	mc := memcache.New("127.0.0.1:11211")

	file, err := os.Open("/home/jmurillo/SHARDS-C/Traces/YT.dat")

	if err != nil {
        log.Fatal(err)
    }
    defer file.Close()
    
    var object string

    

    scanner := bufio.NewScanner(file)

    //var item* memcache.Item 

    for scanner.Scan() {
    	object = scanner.Text()

    	fmt.Println("Set: " +object)
    	mc.Set(&memcache.Item{Key: object, Value: []byte("my value")})
    	
        /*
        item, err = mc.Get(object)
    	
        if err != nil {
        	log.Fatal(err)
    	}
    	fmt.Println("Get: " +item.Key)
        */
    }

    
    



    
    

 
}