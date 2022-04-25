## Service description

The data storage service based on raft can realize the data consistency storage of 2n + 1 nodes.
The storage service has two servants:

- RaftObj: raft port interface
- CountObj: business service module

The user can call the CountObj interface, RaftObj is provided for raft protocol negotiation

## Configuration file

The service configuration file format is as follows:

```xml

<root>
    <raft>
        #Election timeout (MS)
        electionTimeoutMilliseconds = 3000
        #Heartbeat time between leader and slave (MS)
        heartbeatPeriodMilliseconds = 300
        #Time taken to snapshot data (MS)
        snapshotPeriodSeconds = 6000
        #Maximum number of logs per request when synchronizing data
        maxLogEntriesPerRequest = 100
        #The maximum number of data pieces in the memory queue when synchronizing data
        maxLogEntriesMemQueue = 3000
        #The maximum number of pieces of data being transmitted when the same data
        maxLogEntriesTransfering = 1000
    </raft>

    #Data path
    start-count = 100000
    storage-path=/count-data

</root>

```

## Deployment description

The deployment focuses on:

- At least (2n + 1) nodes and at least three nodes need to be deployed
- After 2N-1 nodes are down, the cluster will not be affected and can still work normally
- When adding nodes on the tars framework, please note that one node is added one by one. Do not add multiple nodes at the same time. Observe that the log nodes are normal before adding the next one
- Adding a new node will automatically synchronize the data. In principle, no processing is required
- The same goes for reducing nodes
- The data directory is specified in the configuration file
- If you deploy using k8sframework mechanism, please note that you should enable localpv support and increase the mount path (mount data directory)

## Instructions for use

- Using C++, the underlying data storage uses rockesdb
- Get the tars protocol file and call the service through CountPrx 
- The technical interface 'count' and the data interface are finally forwarded to the leader to perform write processing
- The query data interface can specify whether to execute in the leader. If the leader is not specified, it is not guaranteed that the written data can be found immediately, because there is a certain delay in data synchronization
- If the query specifies the leader every time, it will affect the efficiency, but if the leader is not specified, there will be data delay (usually in milliseconds)
- You can send commands to view and modify the data of the table on tarsweb to facilitate debugging
  > - count.get sBusinessName sKey
  > - count.set sBusinessName sKey value
