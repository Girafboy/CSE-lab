# Lab-4: Cache for Data  



### **Due: 11-20-2019 23:59 (****UTC****+8)**


**% mkdir newdir; cd newdir; (enter a clean directory)**

**% git clone https://ipads.se.sjtu.edu.cn:1312/lab/cse-2019-fall.git;**

**% cd cse-2019-fall; git checkout lab4;**



**Introduction**


 In lab3, you have built a lock server and client that cache locks at the client, reducing the load on the server and improving client performance. In this lab, you will build a cache for all rpc requests. In lab2, you have integrated RPC with YFS. All operations in extent_client.cc are handled by RPC requests. In this lab, you are requested to build a data cache for all these operations to improve performance.

The challenge in the lab is the protocol between the clients and the server. For example, when client 2 modifies the data that client 1 has cached, the server must invalidate the cached data on client 1 by sending a invalidate RPC to client 1 or client 1 only modifies the cached data without synchronize to the server and client 2 requested the newest version, etc. The protocol is further complicated by the fact that concurrent RPC requests and replies may not be delivered in the same order in which they were sent.
 
You should design your own caching protocol, please make sure the protocol is correct and reduces the amount of RPC traffic as many as possible. 
 
We'll test your caching extent_server and client by seeing whether it reduces the amount of RPC traffic that your yfs_client generates. 

If you have questions about this lab, either in programming environment or requirement, please ask TA: Fuqian Huang(hfq_zyx@163.com).


* * *

**Getting started**


**At first, please remember to save your lab3 solution:**


**% cd lab-cse/lab**
** **
**% git commit -a -m “solution for lab3” **
 
Then, pull from the repository:



**% git pull**
remote: Counting objects: 43, done.
…

...
 * [new branch]      lab4      -&gt; origin/lab4
Already up-to-date
 
Then, change to lab4 branch:
 
**% git checkout lab4**

 
Merge with lab3, and solve the conflict by yourself (Git may not be able to figure out how to merge your changes with the new lab assignment (e.g., if you modified some of the code that the second lab assignment changes). In that case, git merge will tell you which files have conflicts, and you should first resolve the conflicts (by editing the relevant files) and then run 'git commit -a'):
 
**% git merge lab3**
Auto-merging fuse.cc
CONFLICT (content): Merge conflict in yfs_client.cc
Auto-merging yfs_client.cc
CONFLICT (content): Merge conflict in ifs_client.cc
Automatic merge failed; fix conflicts and then commit the result
…
 
After merge all of the conflicts, you should be able to compile successfully:
 
**% make**

 
There are no new files added to your lab directory compared to lab3.




* * *


**Your Job**


**Step One: Design the Protocol**

As you have finished lab3 on your own, you should be familiar with how to design a cache.

So there is no hint or recommendation for this lab. :)

If you did not learn anything from lab3 (the lab was not finished by your own or you suffer from amnesia due to poor sleep), please contact TA. 
The TA will invalidate your lab3 score and you should redo your lab3.

**Step Two: Test your design, Testing with RPC_LOSSY=0**

Once you have your full protocol implemented, you can run it using the ./grade.sh script, just as in Lab 2. For now, don't bother testing with loss:

**% export RPC_LOSSY=0
**

**% ./grade.sh**

 

**Step Three: Test your design with RPC_LOSSY=5**

Now that it works without loss, you should try testing with RPC_LOSSY=5. Here you may discover problems with reordered RPCs and responses.


**% export RPC_LOSSY=5
**

**% ./grade.sh**

 

**Evaluation Criteria**

Our measure of performance is the number of RPCs sent to your extent_server generates.

Recall lab3, the RPC library has a feature that counts unique RPCs arriving at the server. You can set the environment variable RPC_COUNT to N before you launch a server process, and it will print out RPC statistics every N RPCs. For example, in the bash shell you could do:


**% export RPC_COUNT=25
**

**% ./grade.sh**


RPC STATS: ... will be printed into extent_server.log

...


We will check the following:


*   Your caching design passes the ./grade.sh test script with RPC_LOSSY=0 and RPC_LOSSY=5.
*   Your lab 4 code generates about a tenth as many RPCs as your lab 2 code on ./grade.sh. 


* * *



**Evil Part (Optional)**


**Design your own testcases**

As the test script on this lab is the same with lab2, it may be too easy for you guys. (Excellent students of top3 university) 

To make the lab more interesting and challenging, you can design your own testcases.

You can submit your testcases in a **test** directory and packed it in .tgz file. 

You will get bonus points if your testcases fulfill the requirements:



*   Your lab4 code could pass you own testcases. 
*   Your testcases is reasonable and easy to run (just run a scripts like ./grade.sh). 
*   Your testcases is well-documented and correct. (TA is not an excellent student as you guys, please make it clear in the document why your testcases is correct and reasonable.), you can write the document in a markdown or well comment the code.  


**Bonus Criteria**

If your testcases is correct and reasonable, and your lab4 code could pass them. Your testcases can be used to test other students' code. 

If there are N students' code fail to pass your testcases. You will get N * 0.1 bonus point in the final and those N students will lose 0.1 point. 
 For example, if there are 37 students do not pass your testcases, and your code fail to pass testcases from other 5 students, you will finally get bonus points of (3.7 - 0.5) = 3.2.



* * *


**Handin procedure**
(You will get 0 score if your submitted file is larger than 5MB.)

After all above done:
 
**% make handin**



That should produce a file called lab4.tgz in the directory. Change the file name to your student id:
 
**% mv lab4.tgz lab4_[your student id].tgz**

 
Then upload lab4_[your student id].tgz file to ftp://SJTU.Ticholas.Huang:public@public.sjtu.edu.cn/upload/cse/lab4 before the deadline. You are only given the permission to list and create new file, but no overwrite and read. So make sure your implementation has passed all the tests before final submit.

You will receive 80% credit if your software passes the same tests we gave you when we run your software on our machines, other credit will be given you after TA reviewing your lab code.
