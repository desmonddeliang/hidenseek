using UnityEngine;
using System;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;
using System.IO;
using UnityEngine.UI;
using System.Text;
using System.Linq;
using System.Runtime.InteropServices;

public struct hns_init
{
    public UInt32 type;
    public UInt32 role;
    public UInt32 id;
}

public struct hns_update
{
    public UInt32 type;
    public UInt32 id;
    public float x;
    public float y;
    public UInt32 status;
}

public struct hns_type
{
    public UInt32 type;
}

public struct hns_init_ack
{
    public UInt32 ack_code;
}

public struct hns_game_start
{
    public UInt32 num_players;
}

public struct hns_uint32
{
    public UInt32 data;
}

public struct hns_broadcast_hdr
{
    public UInt32 type;
    public UInt32 num_players;
    public UInt32 game_status;
    public UInt32 points;
    public UInt64 timestamp;
}

public struct hns_broadcast_player
{
    public UInt32 role;
    public float x;
    public float y;
    public UInt32 status;
}

public class client: MonoBehaviour
{
    internal Boolean socketReady = false;
    TcpClient mySocket;
    UdpClient udpSocket;
    IPEndPoint hostEndPoint;
    NetworkStream theStream;
    StreamWriter theWriter;
    StreamReader theReader;
    String Host = "127.0.0.1";
    Int32 TCP_Port = 54321;
    Int32 UDP_Port = 54329;

    float smooth_speed = 1.5f;
    float killer_smooth_speed = 1.7f;

    public UInt32 my_id = 0;
    public UInt32 my_role = 0;
    public UInt32 my_status = 0;
    public UInt32 game_status = 0;

    Vector3 p0_pos = new Vector3(0, 0, 0);
    Vector3 p1_pos = new Vector3(3, 3, 0);
    Vector3 p2_pos = new Vector3(3, -3, 0);
    Vector3 p3_pos = new Vector3(-3, 3, 0);
    Vector3 p4_pos = new Vector3(-3, -3, 0);

    void Start()
    {

    }

    void Update()
    {
        if (game_status == 0)
        {
            read_update();
        }
        if (my_role != 0)
        {
            GameObject.Find("lion").transform.position = Vector3.MoveTowards(GameObject.Find("lion").transform.position, p0_pos, killer_smooth_speed * Time.deltaTime);
        }
        if (my_role != 1)
        {
            GameObject.Find("chicken").transform.position = Vector3.MoveTowards(GameObject.Find("chicken").transform.position, p1_pos, smooth_speed * Time.deltaTime);
        }
        if (my_role != 2)
        {
            GameObject.Find("cat").transform.position = Vector3.MoveTowards(GameObject.Find("cat").transform.position, p2_pos, smooth_speed * Time.deltaTime);
        }
        if (my_role != 3)
        {
            GameObject.Find("pig").transform.position = Vector3.MoveTowards(GameObject.Find("pig").transform.position, p3_pos, smooth_speed * Time.deltaTime);
        }
        if (my_role != 4)
        {
            GameObject.Find("dog").transform.position = Vector3.MoveTowards(GameObject.Find("dog").transform.position, p4_pos, smooth_speed * Time.deltaTime);
        }


    }
    // **********************************************




    public void init_game(string hostname)
    {
        // create my random id
        System.Random rnd = new System.Random();
        my_id = (UInt32)rnd.Next(1, 99999);
        Debug.Log("MY ID" + my_id.ToString());

        // Connect to server
        Host = hostname;
        Debug.Log("Trying to connect to server:" + Host);
        setupTCP();

        // Set misc
        Application.runInBackground = true;

        // UI
        GameObject.Find("btn_connect_server").SetActive(false);
        GameObject.Find("btn_connect_localhost").SetActive(false);
        GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Waiting for server response";

        // Send Init
        send_init();

    }

    public void setupTCP()
    {
        try
        {
            mySocket = new TcpClient(Host, TCP_Port);
            theStream = mySocket.GetStream();
            theWriter = new StreamWriter(theStream);
            theReader = new StreamReader(theStream);
            socketReady = true;
        }
        catch (Exception e)
        {
            Debug.Log("Socket error: " + e);
            GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Error connecting to server";
        }
    }

    public void setupUDP()
    {
        /* setting up UDP socket for game data transfering */
        IPAddress serverIP = IPAddress.Parse(Host);
        hostEndPoint = new IPEndPoint(serverIP, UDP_Port);
        udpSocket = new UdpClient();
        udpSocket.Connect(hostEndPoint);
        udpSocket.Client.Blocking = false;

        /* start game */
        InvokeRepeating("send_update", 1.0f, 0.01f);
        udpSocket.BeginReceive(new AsyncCallback(processDgram), udpSocket);

    }

    public void processDgram(IAsyncResult res)
    {
        /* everytime we gets a UDP packet from the server, we unpack and process it */
        try
        {
            byte[] received = udpSocket.EndReceive(res, ref hostEndPoint);
      
            hns_broadcast_hdr bro = fromBytestoBroadcastHdr(received.Take(24).ToArray());
            UInt32 num_players = bro.num_players;
            GameObject.Find("txt_points").GetComponentInChildren<Text>().text = "Points: " + (int)(bro.points) + "/" + (num_players-1) * 3000;


            for(int i=0; i<num_players; i++)
            {
                hns_broadcast_player bro_player = fromBytestoBroadcastPlayer(received.Skip(i* 16 + 24).Take(16).ToArray());

                if (bro_player.role == my_role)
                {
                    if(my_status != bro_player.status & bro_player.status % 2 == 1)
                    {
                        GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "You are frozen! wait for other player to free you!";
                    }
                    if (my_status != bro_player.status & bro_player.status % 2 == 0)
                    {
                        GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "You are freed!";
                    }

                    my_status = bro_player.status;
                    GameObject.Find("txt_health").GetComponentInChildren<Text>().text = "Lives: " + (int)((6 - my_status) / 2);
                    
                    if (my_status >= 5)
                    {
                        game_status = 2;
                        GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "You lost the game! Others can still win.";
                    }
                }

                switch (bro_player.role)
                {
                    case 0: /* Killer */
                        
                        if(bro_player.role != my_role)
                        {
                            GameObject.Find("lion").transform.position = p0_pos;
                        }
                        p0_pos = new Vector3(bro_player.x, bro_player.y, 0);
                        break;
                    case 1:
                        if (bro_player.role != my_role)
                        {
                            GameObject.Find("chicken").transform.position = p1_pos;
                        }
                        p1_pos = new Vector3(bro_player.x, bro_player.y, 0);
                        break;
                    case 2:
                        if (bro_player.role != my_role)
                        {
                            GameObject.Find("cat").transform.position = p2_pos;
                        }
                        p2_pos = new Vector3(bro_player.x, bro_player.y, 0);
                        break;
                    case 3:
                        if (bro_player.role != my_role)
                        {
                            GameObject.Find("pig").transform.position = p3_pos;
                        }
                        p3_pos = new Vector3(bro_player.x, bro_player.y, 0);
                        break;
                    case 4:
                        if (bro_player.role != my_role)
                        {
                            GameObject.Find("dog").transform.position = p4_pos;
                        }
                        p4_pos = new Vector3(bro_player.x, bro_player.y, 0);
                        break;
                }

            }

            if (bro.game_status == 3)
            {
                /* Game over, survivor won */
                game_status = 3;
                GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Game Over! Animals won!";
            }

            if (bro.game_status == 4)
            {
                /* Game over, killer won */
                game_status = 4;
                GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Game Over! Lion Won!";
            }

            udpSocket.BeginReceive(new AsyncCallback(processDgram), udpSocket);
        }
        catch (Exception ex)
        {
            throw ex;
        }
    }


    public void send_update()
    {
        if (game_status == 1)
        {
            /* Game start, send my info */
            hns_update my_update = new hns_update();
            my_update.type = 3;
            my_update.id = my_id;

            switch (my_role)
            {
                case 0: /* Killer */
                    my_update.x = GameObject.Find("lion").transform.position.x;
                    my_update.y = GameObject.Find("lion").transform.position.y;
                    break;
                case 1:
                    my_update.x = GameObject.Find("chicken").transform.position.x;
                    my_update.y = GameObject.Find("chicken").transform.position.y;
                    break;
                case 2:
                    my_update.x = GameObject.Find("cat").transform.position.x;
                    my_update.y = GameObject.Find("cat").transform.position.y;
                    break;
                case 3:
                    my_update.x = GameObject.Find("pig").transform.position.x;
                    my_update.y = GameObject.Find("pig").transform.position.y;
                    break;
                case 4:
                    my_update.x = GameObject.Find("dog").transform.position.x;
                    my_update.y = GameObject.Find("dog").transform.position.y;
                    break;
            }

            udpSocket.Send(getBytesfromUpdate(my_update), 20);
        }

    }

    public void send_init()
    {
        /* send init packet to server for role assignment */
        hns_init my_init = new hns_init();

        my_init.type = 0;
        my_init.id = my_id;
        my_init.role = 0;

        if (!socketReady)
            return;

        theWriter.BaseStream.Write(getBytes(my_init), 0, 12);
        theWriter.Flush();
    }


    public int read_update()
    {
        if (!socketReady)
            return 0;
        if (!theStream.DataAvailable)
            return 0;

        hns_uint32 packetType;
        byte[] type_buf = new byte[4];

        theReader.BaseStream.Read(type_buf, 0, 4);
        packetType = fromBytestoUint32(type_buf);

        /* Type 1: Server ACK to Client init */
        if (packetType.data == 1)
        {
            byte[] read_buf = new byte[4];
            theReader.BaseStream.Read(read_buf, 0, 4);
            hns_uint32 init_ack = fromBytestoUint32(read_buf);
            if (init_ack.data == 0)
            {
                my_role = init_ack.data;
                GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Waiting for game start, you are a lion, protect the circles and eat other animals!";
            } else if (init_ack.data >= 1 & init_ack.data <= 4)
            {
                my_role = init_ack.data;
                GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Waiting for game start, avoid the lion and stay in the circles as long as possible!";
            } else
            {
                GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Invalid connection!";
            }
        }

        /* Type 2: Server Game Start Signal */
        if (packetType.data == 2)
        {
            byte[] read_buf = new byte[4];
            theReader.BaseStream.Read(read_buf, 0, 4);
            hns_uint32 game_start = fromBytestoUint32(read_buf);
            GameObject.Find("txt_banner").GetComponentInChildren<Text>().text = "Game Start with a total of " + game_start.data.ToString() + " players";
            GameObject.Find("txt_health").GetComponentInChildren<Text>().text = "Lives: " + (int)((6 - my_status) / 2);
            game_status = 1;

            /* init player start position */

            switch (my_role)
            {
                case 0: /* Killer */
                    Vector3 temp1 = new Vector3(0, 0, 0);
                    GameObject.Find("lion").SetActive(true);
                    GameObject.Find("lion").transform.position = temp1;
                    GameObject.Find("lion").AddComponent<PlayerMovement>();
                    break;
                case 1:
                    Vector3 temp2 = new Vector3(3, 3, 0);
                    GameObject.Find("chicken").SetActive(true);
                    GameObject.Find("chicken").transform.position = temp2;
                    GameObject.Find("chicken").AddComponent<PlayerMovement>();
                    break;
                case 2:
                    Vector3 temp3 = new Vector3(3, -3, 0);
                    GameObject.Find("cat").SetActive(true);
                    GameObject.Find("cat").transform.position = temp3;
                    GameObject.Find("cat").AddComponent<PlayerMovement>();
                    break;
                case 3:
                    Vector3 temp4 = new Vector3(-3, 3, 0);
                    GameObject.Find("pig").SetActive(true);
                    GameObject.Find("pig").transform.position = temp4;
                    GameObject.Find("pig").AddComponent<PlayerMovement>();
                    break;
                case 4:
                    Vector3 temp5 = new Vector3(-3, -3, 0);
                    GameObject.Find("dog").SetActive(true);
                    GameObject.Find("dog").transform.position = temp5;
                    GameObject.Find("dog").AddComponent<PlayerMovement>();
                    break;
            }


            switch (game_start.data)
            {
                case 2: /* Killer */
                    GameObject.Find("cat").SetActive(false);
                    GameObject.Find("pig").SetActive(false);
                    GameObject.Find("dog").SetActive(false);
                    break;
                case 3:
                    GameObject.Find("pig").SetActive(false);
                    GameObject.Find("dog").SetActive(false);
                    break;
                case 4:
                    GameObject.Find("dog").SetActive(false);
                    break;
            }

            setupUDP();

        }


        theReader.BaseStream.Flush();
        return 0;
    }
    public void closeSocket()
    {
        if (!socketReady)
            return;
        theWriter.Close();
        theReader.Close();
        mySocket.Close();
        socketReady = false;
    }

    /* Below are all bytes to struct (vice versa) conversion functions */

    public byte[] getBytes(hns_init str)
    {
        int size = Marshal.SizeOf(str);
        byte[] arr = new byte[size];

        IntPtr ptr = Marshal.AllocHGlobal(size);
        Marshal.StructureToPtr(str, ptr, true);
        Marshal.Copy(ptr, arr, 0, size);
        Marshal.FreeHGlobal(ptr);
        return arr;
    }

    public byte[] getBytesfromUpdate(hns_update str)
    {
        int size = Marshal.SizeOf(str);
        byte[] arr = new byte[size];

        IntPtr ptr = Marshal.AllocHGlobal(size);
        Marshal.StructureToPtr(str, ptr, true);
        Marshal.Copy(ptr, arr, 0, size);
        Marshal.FreeHGlobal(ptr);
        return arr;
    }


    public hns_uint32 fromBytestoUint32(byte[] arr)
    {
        hns_uint32 str = new hns_uint32();

        int size = Marshal.SizeOf(str);
        IntPtr ptr = Marshal.AllocHGlobal(size);

        Marshal.Copy(arr, 0, ptr, size);

        str = (hns_uint32)Marshal.PtrToStructure(ptr, str.GetType());
        Marshal.FreeHGlobal(ptr);

        return str;
    }


    public hns_broadcast_hdr fromBytestoBroadcastHdr(byte[] arr)
    {
        hns_broadcast_hdr str = new hns_broadcast_hdr();

        int size = Marshal.SizeOf(str);
        IntPtr ptr = Marshal.AllocHGlobal(size);

        Marshal.Copy(arr, 0, ptr, size);

        str = (hns_broadcast_hdr)Marshal.PtrToStructure(ptr, str.GetType());
        Marshal.FreeHGlobal(ptr);

        return str;
    }

    public hns_broadcast_player fromBytestoBroadcastPlayer(byte[] arr)
    {
        hns_broadcast_player str = new hns_broadcast_player();

        int size = Marshal.SizeOf(str);
        IntPtr ptr = Marshal.AllocHGlobal(size);

        Marshal.Copy(arr, 0, ptr, size);

        str = (hns_broadcast_player)Marshal.PtrToStructure(ptr, str.GetType());
        Marshal.FreeHGlobal(ptr);

        return str;
    }


} // end class s_TCP