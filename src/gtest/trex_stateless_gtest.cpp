/*
 Hanoh Haim
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "bp_sim.h"
#include <common/gtest.h>
#include <common/basic_utils.h>
#include <trex_stateless_dp_core.h>
#include <trex_stateless_messaging.h>
#include <trex_streams_compiler.h>
#include <trex_stream_node.h>
#include <trex_stream.h>
#include <trex_stateless_port.h>
#include <trex_rpc_server_api.h>


#define EXPECT_EQ_UINT32(a,b) EXPECT_EQ((uint32_t)(a),(uint32_t)(b))



/* basic stateless test */
class basic_stl  : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
public:
};



class CBasicStl {

public:
    CBasicStl(){
        m_time_diff=0.001;
        m_threads=1;
        m_dump_json=false;
    }

    bool  init(void){

        CErfIFStl erf_vif;
        fl.Create();
        fl.generate_p_thread_info(1);
        CFlowGenListPerThread   * lpt;

        fl.m_threads_info[0]->set_vif(&erf_vif);

        CErfCmp cmp;
        cmp.dump=1;

        CMessagingManager * cp_dp = CMsgIns::Ins()->getCpDp();

        m_ring_from_cp = cp_dp->getRingCpToDp(0);


        bool res=true;

        lpt=fl.m_threads_info[0];

        char buf[100];
        char buf_ex[100];
        sprintf(buf,"%s-%d.erf",CGlobalInfo::m_options.out_file.c_str(),0);
        sprintf(buf_ex,"%s-%d-ex.erf",CGlobalInfo::m_options.out_file.c_str(),0);

        lpt->start_stateless_simulation_file(buf,CGlobalInfo::m_options.preview);

        /* add stream to the queue */
        assert(m_msg);

        assert(m_ring_from_cp->Enqueue((CGenNode *)m_msg)==0);

        lpt->start_stateless_daemon_simulation();

        //lpt->m_node_gen.DumpHist(stdout);

        cmp.d_sec = m_time_diff;
        if ( cmp.compare(std::string(buf),std::string(buf_ex)) != true ) {
            res=false;
        }

        if ( m_dump_json ){
            printf(" dump json ...........\n");
            std::string s;
            fl.m_threads_info[0]->m_node_gen.dump_json(s);
            printf(" %s \n",s.c_str());
        }

        fl.Delete();
        return (res);
    }


public:
    int           m_threads;
    double        m_time_diff;
    bool          m_dump_json;
    TrexStatelessCpToDpMsgBase * m_msg;
    CNodeRing           *m_ring_from_cp;
    CFlowGenList  fl;
};


class CPcapLoader {
public:
    CPcapLoader();
    ~CPcapLoader();


public:
    bool load_pcap_file(std::string file,int pkt_id=0);
    void update_ip_src(uint32_t ip_addr);
    void clone_packet_into_stream(TrexStream * stream);
    void dump_packet();

public:
    bool                    m_valid;
    CCapPktRaw              m_raw;
    CPacketIndication       m_pkt_indication;
};

CPcapLoader::~CPcapLoader(){
}

bool CPcapLoader::load_pcap_file(std::string cap_file,int pkt_id){
    m_valid=false;
    CPacketParser parser;

    CCapReaderBase * lp=CCapReaderFactory::CreateReader((char *)cap_file.c_str(),0);

    if (lp == 0) {
        printf(" ERROR file %s does not exist or not supported \n",(char *)cap_file.c_str());
        return false;
    }

    int cnt=0;
    bool found =false;


    while ( true ) {
        /* read packet */
        if ( lp->ReadPacket(&m_raw) ==false ){
            break;
        }
        if (cnt==pkt_id) {
            found = true;
            break;
        }
        cnt++;
    }
    if ( found ){
        if ( parser.ProcessPacket(&m_pkt_indication, &m_raw) ){
            m_valid = true;
        }
    }

    delete lp;
    return (m_valid);
}

void CPcapLoader::update_ip_src(uint32_t ip_addr){

    if ( m_pkt_indication.l3.m_ipv4 ) {
        m_pkt_indication.l3.m_ipv4->setSourceIp(ip_addr);
        m_pkt_indication.l3.m_ipv4->updateCheckSum();
    }
}           

void CPcapLoader::clone_packet_into_stream(TrexStream * stream){

    uint16_t pkt_size=m_raw.getTotalLen();

    uint8_t      *binary = new uint8_t[pkt_size];
    memcpy(binary,m_raw.raw,pkt_size);
    stream->m_pkt.binary = binary;
    stream->m_pkt.len    = pkt_size;
}




CPcapLoader::CPcapLoader(){

}

void CPcapLoader::dump_packet(){
    if (m_valid ) {
        m_pkt_indication.Dump(stdout,1);
    }else{
        fprintf(stdout," no packets were found \n");
    }
}


TEST_F(basic_stl, load_pcap_file) {
    printf (" stateles %d \n",(int)sizeof(CGenNodeStateless));

    CPcapLoader pcap;
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000001);
    pcap.load_pcap_file("cap2/udp_64B.pcap",0);
    pcap.update_ip_src(0x10000001);

    //pcap.dump_packet();
}


TEST_F(basic_stl, single_pkt_burst1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_single_pkt_burst1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStreamBurst(0,0,5, 1.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     TrexStreamsCompiledObj comp_obj(0,1.0);

     comp_obj.set_simulation_duration( 10.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}




TEST_F(basic_stl, single_pkt) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_single_stream";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStreamContinuous(0,0,1.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;


     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);


     streams.push_back(stream1);

     // stream - clean 

     TrexStreamsCompiledObj comp_obj(0,1.0);

     comp_obj.set_simulation_duration( 10.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic_stl, multi_pkt1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_multi_pkt1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStreamContinuous(0,0,1.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     TrexStream * stream2 = new TrexStreamContinuous(0,1,2.0);
     stream2->m_enabled = true;
     stream2->m_self_start = true;
     stream2->m_isg_usec = 1000.0; /* 1 msec */
     pcap.update_ip_src(0x20000001);
     pcap.clone_packet_into_stream(stream2);

     streams.push_back(stream2);


     // stream - clean 
     TrexStreamsCompiledObj comp_obj(0,1.0);

     comp_obj.set_simulation_duration( 10.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}





/* check disabled stream with multiplier of 5*/
TEST_F(basic_stl, multi_pkt2) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_multi_pkt2";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;

     TrexStream * stream1 = new TrexStreamContinuous(0,0,1.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);


     TrexStream * stream2 = new TrexStreamContinuous(0,1,2.0);
     stream2->m_enabled = false;
     stream2->m_self_start = false;
     stream2->m_isg_usec = 1000.0; /* 1 msec */
     pcap.update_ip_src(0x20000001);
     pcap.clone_packet_into_stream(stream2);

     streams.push_back(stream2);


     // stream - clean 
     TrexStreamsCompiledObj comp_obj(0,5.0);

     comp_obj.set_simulation_duration( 10.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );

     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;
     delete stream2 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}


TEST_F(basic_stl, multi_burst1) {

    CBasicStl t1;
    CParserOption * po =&CGlobalInfo::m_options;
    po->preview.setVMode(7);
    po->preview.setFileWrite(true);
    po->out_file ="exp/stl_multi_burst1";

     TrexStreamsCompiler compile;


     std::vector<TrexStream *> streams;



     TrexStream * stream1 = new TrexStreamMultiBurst(0,0,5, 1.0,3,2000000.0);
     stream1->m_enabled = true;
     stream1->m_self_start = true;

     CPcapLoader pcap;
     pcap.load_pcap_file("cap2/udp_64B.pcap",0);
     pcap.update_ip_src(0x10000001);
     pcap.clone_packet_into_stream(stream1);

     streams.push_back(stream1);

     TrexStreamsCompiledObj comp_obj(0,1.0);

     comp_obj.set_simulation_duration( 40.0);
     assert(compile.compile(streams, comp_obj) );

     TrexStatelessDpStart * lpstart = new TrexStatelessDpStart( comp_obj.clone() );


     t1.m_msg = lpstart;

     bool res=t1.init();

     delete stream1 ;

     EXPECT_EQ_UINT32(1, res?1:0)<< "pass";
}




