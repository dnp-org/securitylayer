/* Copyright 2019  Ronald Landheer-Cieslak
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License. */
#include "catch.hpp"
#include "../outstation.hpp"
#include "../master.hpp"
#include "deterministicrandomnumbergenerator.hpp"

static_assert(DNP3SAV6_PROFILE_HPP_INCLUDED, "profile.hpp should be pre-included in CMakeLists.txt");

using namespace std;
using namespace boost::asio;
using namespace DNP3SAv6;

/* The purpose of this test is to test, in detail, an entire handshake instigated by the Oustation.
 * We do byte-by-byte comparisons with expected messages here, so we don't have to in subsequent tests. */
SCENARIO( "Outstation sends an initial unsolicited response" "[unsol]") {
	GIVEN( "An Outstation stack" ) {
		io_context ioc;
		Config default_config;
		Tests::DeterministicRandomNumberGenerator rng;
		Outstation outstation(ioc, 0/* association ID */, default_config, rng);

		THEN( "The Outstation will be in the INITIAL state" ) {
			REQUIRE( outstation.getState() == Outstation::initial__ );
		}

		WHEN( "the Application Layer tries to send an APDU" ) {
			unsigned char apdu_buffer[2048];
			mutable_buffer apdu(apdu_buffer, sizeof(apdu_buffer));
			rng.generate(apdu); // NOTE: we really don't care about the contents of the APDU here
			outstation.postAPDU(apdu);
			THEN( "The Outstation state will be EXPECT_SESSION_START_REQUEST" ) {
				REQUIRE( outstation.getState() == Outstation::expect_session_start_request__ );
			}
			THEN( "The Outstation will attempt to send a SessionInitiation message" ) {
				REQUIRE( outstation.pollSPDU() );
				auto spdu(outstation.getSPDU());
				REQUIRE( !outstation.pollSPDU() );
				REQUIRE( spdu.size() == (8/*SPDU header size*/) );
				unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                unsigned char const expected_header_bytes[] = {
                      0xc0, 0x80, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00
                    };
                REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
			}
			THEN( "The outstation statistics should be OK" ) {
				REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			}
			WHEN( "The Master receives it" ) {
				Master master(ioc, 0/* association ID */, default_config, rng);
				REQUIRE( master.getState() == Master::initial__ );
				
				auto spdu(outstation.getSPDU());
				master.postSPDU(spdu);
				THEN( "The Master should be in the EXPECT_SESSION_START_RESPONSE state" ) {
					REQUIRE( master.getState() == Master::expect_session_start_response__ );
				}
			    THEN( "The Master statistics should be OK" ) {
				    REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 1 );
				    REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 1 );
				    REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			    }
				THEN( "The Master should not present anything as an APDU" ) {
					REQUIRE( !master.pollAPDU() );
				}
				THEN( "The Master should send back a SessionStartRequest" ) {
					REQUIRE( master.pollSPDU() );
					auto spdu(master.getSPDU());
					REQUIRE( !master.pollSPDU() );
					REQUIRE( spdu.size() == 10 );
					unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                    unsigned char const expected_header_bytes[] = {
                          0xc0, 0x80, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );

                    unsigned char const expected_payload_bytes[] = {
                          0x06, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected_payload_bytes, sizeof(expected_payload_bytes)) == 0 );
				}
				//TODO test cases where the Outstation sent its RequestSessionInitation message with sequence numbers 
				//     other than 1, according to OPTION_IGNORE_OUTSTATION_SEQ_ON_REQUEST_SESSION_INITIATION
				WHEN( "The Outstation receives the Master's response" ) {
					spdu = master.getSPDU();
					outstation.postSPDU(spdu);
					THEN( "The Outstation should go to the EXPECT_SET_KEYS state" ) {
						REQUIRE( outstation.getState() == Outstation::expect_session_key_change_request__ );
					}
			        THEN( "The outstation statistics should be OK" ) {
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
					THEN( "The Outstation should not present an APDU" ) {
						REQUIRE( !outstation.pollAPDU() );
					}
					THEN( "The Outstation should present an SPDU" ) {
						REQUIRE( outstation.pollSPDU() );
					}
					THEN( "The Outstation should send back a SessionStartResponse" ) {
						REQUIRE( outstation.pollSPDU() );
						auto spdu(outstation.getSPDU());
						REQUIRE( !outstation.pollSPDU() );
						REQUIRE( spdu.size() == 13 );
						unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                        unsigned char const expected_header_bytes[] = {
                              0xc0, 0x80, 0x01, 0x03, 0x00, 0x00, 0x01, 0x00
                            };
                        REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                        unsigned char const expected_payload[] = {
                              0x04
                            , 0x79, 0x28, 0x11, 0xc8
                            };
                        REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected_payload, sizeof(expected_payload)) == 0 );
					}
                    WHEN( "The Outstation sends a SessionStartResponse" ) {
                        outstation.getSPDU();
			            THEN( "The outstation statistics should be OK" ) {
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
                    }
					WHEN( "The Master receives the SessionStartResponse" ) {
						auto spdu(outstation.getSPDU());
						master.postSPDU(spdu);
						THEN( "The Master should go to the EXPECT_SESSION_ACK state" ) {
							REQUIRE( master.getState() == SecurityLayer::expect_session_key_change_response__ );
						}
			            THEN( "The Master statistics should be OK" ) {
				            REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				            REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
				        THEN( "The Master should not present anything as an APDU" ) {
					        REQUIRE( !master.pollAPDU() );
				        }
						THEN( "The Master will send SessionKeyChangeRequest message" ) {
							auto spdu(master.getSPDU());
							REQUIRE( spdu.size() == 100 );
							unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                            unsigned char const expected_header_bytes[] = {
                                  0xc0, 0x80, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00
                                };
                            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
							unsigned char const expected[] = {
                                  0x02, 0x04, 0x58, 0x00
								, 0x19, 0x3c, 0xb4, 0xf8, 0x7d, 0x8a, 0xb4, 0x61, 0x94, 0x58, 0x79, 0x14, 0xa0, 0xbf, 0x8e, 0x1a
								, 0xb5, 0xff, 0x2b, 0xc9, 0x7e, 0xbc, 0x25, 0x86, 0xd0, 0xee, 0x7e, 0x22, 0xb3, 0xbe, 0x78, 0xeb
								, 0x31, 0x09, 0x86, 0x06, 0x14, 0xb6, 0x7b, 0x9a, 0xd9, 0xb7, 0x1c, 0x0f, 0x06, 0xe7, 0x40, 0xcb
								, 0x7d, 0x5b, 0x43, 0xb8, 0x90, 0x1b, 0x49, 0xde, 0xeb, 0x9b, 0xad, 0x8f, 0x1f, 0x80, 0xe8, 0xd6
								, 0xdb, 0x29, 0x9b, 0xe2, 0xa0, 0x7e, 0x7d, 0x72, 0xb2, 0x92, 0x10, 0xdb, 0xdd, 0x59, 0x68, 0xfc
								, 0x5f, 0x5d, 0x54, 0x54, 0xdc, 0xbf, 0x79, 0xa3
                                };
							static_assert(sizeof(expected) == 92, "unexpected size for expected response");
							REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected, sizeof(expected)) == 0 );
						}
						//TODO check invalid messages (things that should provoke error returns)
						//TODO check with the wrong sequence number
                        WHEN( "The Outstation receives the SessionKeyChangeRequest message" ) {
                            auto spdu(master.getSPDU());
                            outstation.postSPDU(spdu);
                            THEN( "The outstation should go to the ACTIVE state" ) {
        						REQUIRE( outstation.getState() == Outstation::active__ );
                            }
					        THEN( "The Outstation should not present an APDU" ) {
						        REQUIRE( !outstation.pollAPDU() );
					        }
			                THEN( "The outstation statistics should be OK" ) {
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 3 );
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                }
                            THEN( "The Outstation will attempt to send a SessionKeyChangeResponse message" ) {
                                REQUIRE( outstation.pollSPDU() );
				                auto spdu(outstation.getSPDU());
				                REQUIRE( !outstation.pollSPDU() );
				                REQUIRE( spdu.size() == 24 );
				                unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                                unsigned char const expected_header_bytes[] = {
                                      0xc0, 0x80, 0x01, 0x05, 0x00, 0x00, 0x01, 0x00
                                    };
                                REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                                unsigned char const expected_payload_bytes[] = {
									  0x83, 0xf2, 0x70, 0x0e, 0xf4, 0x92, 0x56, 0xc8, 0x2b, 0xe2, 0xec, 0x50, 0xa0, 0x9e, 0xa6, 0xe3
                                    };
                                REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected_payload_bytes, sizeof(expected_payload_bytes)) == 0 );
                            }
                            WHEN( "The Outstation to sends a SessionKeyChangeResponse message" ) {
                                auto spdu(outstation.getSPDU());
                                THEN( "The Outstation has prepared the authenticated-APDU SPDU" )
                                {
                                    outstation.update();
                                    REQUIRE( outstation.pollSPDU() ); // for the pending APDU
				                    auto spdu(outstation.getSPDU());
				                    REQUIRE( !outstation.pollSPDU() );
				                    REQUIRE( spdu.size() == 2048+8+2+16 );
				                    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                                    unsigned char const expected_header_bytes[] = {
                                          0xc0, 0x80, 0x01, 0x06, 0x00, 0x00, 0x01, 0x00
                                        };
                                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );

    			                    REQUIRE( spdu_bytes[sizeof(expected_header_bytes) + 0] == 0x00 );
    			                    REQUIRE( spdu_bytes[sizeof(expected_header_bytes) + 1] == 0x08 );
                                    REQUIRE( memcmp(apdu.data(), spdu_bytes + (8/*SPDU header size*/) + 2, apdu.size()) == 0 );

                                    unsigned char const expected_payload_bytes[] = {
                                          0x90, 0x2a, 0x5a, 0x5b, 0x6c, 0x68, 0x57, 0xb5, 0xfd, 0xa8, 0x41, 0x67, 0x4a, 0xcb, 0x19, 0xc2
                                        };

                                    REQUIRE( memcmp(expected_payload_bytes, spdu_bytes + (8/*SPDU header size*/) + 2 + apdu.size(), 16) == 0 );
                                }
			                    THEN( "The outstation statistics should be OK" ) {
				                    REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 3 );
				                    REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                    REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                    }
                                WHEN( "The Master receives it" ) {
                                    master.postSPDU(spdu);
                                    THEN( "The Master should go to the ACTIVE state" ) {
                                        REQUIRE( master.getState() == SecurityLayer::active__ );
                                    }
			                        THEN( "The Master statistics should be OK" ) {
				                        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				                        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 3 );
				                        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                        }
				                    THEN( "The Master should not present anything as an APDU" ) {
					                    REQUIRE( !master.pollAPDU() );
				                    }
                                    WHEN( "The Outstation canceled the APDU (timeout)" ) {
                                        outstation.onApplicationLayerTimeout();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation canceled the APDU (application reset)" ) {
                                        outstation.onApplicationReset();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation canceled the APDU (cancel)" ) {
                                        outstation.cancelPendingAPDU();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation does send the APDU" ) {
                                        outstation.update();
                                        REQUIRE( outstation.pollSPDU() ); // for the pending APDU
				                        auto spdu(outstation.getSPDU());
				                        REQUIRE( !outstation.pollSPDU() );
			                            THEN( "The outstation statistics should be OK" ) {
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 4 );
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 1 );	
				                            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                            }
                                        WHEN( "The Master receives it" ) {
                                            master.postSPDU(spdu);
                                            THEN( "The Master will have an APDU ready for consumption" ) {
                                                REQUIRE( master.pollAPDU() );
                                            }
                                            THEN( "The Master can spit out the same APDU we originally sent" ) {
                                                auto the_apdu(master.getAPDU());
                                                REQUIRE( the_apdu.size() == apdu.size() );
                                                REQUIRE( memcmp(the_apdu.data(), apdu.data(), apdu.size()) == 0 );
                                            }
                                        }
                                    }
                                }
                            }
                        }
					}
				}
				//TODO test that a session start request from a broadcast address is ignored
				//TODO check invalid messages (things that should provoke error returns)
			}
		}
	}
}

/* We'll only check message headers byte-by-byte here to allow for slightly cleaner code */
SCENARIO( "Master sends an initial poll" "[master-init]") {
	GIVEN( "A Master that needs to send an APDU" ) {
		io_context ioc;
		Config default_config;
		Tests::DeterministicRandomNumberGenerator rng;
		Master master(ioc, 0/* association ID */, default_config, rng);
		REQUIRE( master.getState() == Master::initial__ );
		unsigned char apdu_buffer[2048];
		mutable_buffer apdu(apdu_buffer, sizeof(apdu_buffer));
		rng.generate(apdu); // NOTE: we really don't care about the contents of the APDU here
				
		master.postAPDU(apdu);
		THEN( "The Master go to in the EXPECT_SESSION_START_RESPONSE state" ) {
			REQUIRE( master.getState() == Master::expect_session_start_response__ );
		}
		THEN( "The Master statistics should be OK" ) {
			REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 1 );
			REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 0 );
			REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
			REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
			REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
			REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
			static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
		}
		THEN( "The Master should not present anything as an APDU" ) {
			REQUIRE( !master.pollAPDU() );
		}
		THEN( "The Master should send a SessionStartRequest" ) {
			REQUIRE( master.pollSPDU() );
			auto spdu(master.getSPDU());
			REQUIRE( !master.pollSPDU() );
			REQUIRE( spdu.size() == 10 );
			unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
            unsigned char const expected_header_bytes[] = {
                    0xc0, 0x80, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00
                };
            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
		}
        GIVEN( "A newly booted Outstation" ) {
            Outstation outstation(ioc, 0/* association ID */, default_config, rng);
            REQUIRE( outstation.getState() == Outstation::initial__ );

		    WHEN( "The Outstation receives the Master's request" ) {
			    auto spdu(master.getSPDU());
			    outstation.postSPDU(spdu);
			    THEN( "The Outstation should go to the EXPECT_SET_KEYS state" ) {
				    REQUIRE( outstation.getState() == Outstation::expect_session_key_change_request__ );
			    }
			    THEN( "The outstation statistics should be OK" ) {
				    REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				    REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				    REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			    }
			    THEN( "The Outstation should not present an APDU" ) {
				    REQUIRE( !outstation.pollAPDU() );
			    }
			    THEN( "The Outstation should present an SPDU" ) {
				    REQUIRE( outstation.pollSPDU() );
			    }
			    THEN( "The Outstation should send back a SessionStartResponse" ) {
				    REQUIRE( outstation.pollSPDU() );
				    auto spdu(outstation.getSPDU());
				    REQUIRE( !outstation.pollSPDU() );
				    REQUIRE( spdu.size() == 13 );
				    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                    unsigned char const expected_header_bytes[] = {
                            0xc0, 0x80, 0x01, 0x03, 0x00, 0x00, 0x01, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
			    }
                WHEN( "The Outstation sends a SessionStartResponse" ) {
                    outstation.getSPDU();
			        THEN( "The outstation statistics should be OK" ) {
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
                }
			    WHEN( "The Master receives the SessionStartResponse" ) {
				    auto spdu(outstation.getSPDU());
				    master.postSPDU(spdu);
				    THEN( "The Master should go to the EXPECT_SESSION_ACK state" ) {
					    REQUIRE( master.getState() == SecurityLayer::expect_session_key_change_response__ );
				    }
			        THEN( "The Master statistics should be OK" ) {
				        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
				    THEN( "The Master should not present anything as an APDU" ) {
					    REQUIRE( !master.pollAPDU() );
				    }
				    THEN( "The Master will send SessionKeyChangeRequest message" ) {
					    auto spdu(master.getSPDU());
					    REQUIRE( spdu.size() == 100 );
					    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                        unsigned char const expected_header_bytes[] = {
                                0xc0, 0x80, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00
                            };
                        REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
				    }
				    //TODO check invalid messages (things that should provoke error returns)
				    //TODO check with the wrong sequence number
                    WHEN( "The Outstation receives the SessionKeyChangeRequest message" ) {
                        auto spdu(master.getSPDU());
                        outstation.postSPDU(spdu);
                        THEN( "The outstation should go to the ACTIVE state" ) {
        				    REQUIRE( outstation.getState() == Outstation::active__ );
                        }
					    THEN( "The Outstation should not present an APDU" ) {
						    REQUIRE( !outstation.pollAPDU() );
					    }
			            THEN( "The outstation statistics should be OK" ) {
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
                        THEN( "The Outstation will attempt to send a SessionKeyChangeResponse message" ) {
                            REQUIRE( outstation.pollSPDU() );
				            auto spdu(outstation.getSPDU());
				            REQUIRE( !outstation.pollSPDU() );
				            REQUIRE( spdu.size() == 24 );
				            unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                            unsigned char const expected_header_bytes[] = {
                                    0xc0, 0x80, 0x01, 0x05, 0x00, 0x00, 0x01, 0x00
                                };
                            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                        }
                        WHEN( "The Outstation to sends a SessionKeyChangeResponse message" ) {
                            auto spdu(outstation.getSPDU());
			                THEN( "The outstation statistics should be OK" ) {
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                }
                            WHEN( "The Master receives it" ) {
                                master.postSPDU(spdu);
                                THEN( "The Master should go to the ACTIVE state" ) {
                                    REQUIRE( master.getState() == SecurityLayer::active__ );
                                }
			                    THEN( "The Master statistics should be OK" ) {
				                    REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				                    REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				                    REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                    }
				                THEN( "The Master should not present anything as an APDU" ) {
					                REQUIRE( !master.pollAPDU() );
				                }
                                THEN( "The Master is ready to send the APDU" ) {
                                    master.update();
                                    REQUIRE( master.pollSPDU() );
                                }
                                WHEN( "the Master sends the APDU" ) {
                                    master.update();
                                    REQUIRE( master.pollSPDU() );
                                    spdu = master.getSPDU();
                                    REQUIRE( !master.pollSPDU() );
			                        THEN( "The Master statistics should be OK" ) {
				                        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 3 );
				                        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				                        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 1 );	
				                        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                        }
                                    WHEN( "the outstation receives it" ) {
                                        outstation.postSPDU(spdu);
                                        THEN( "the Outstation will present the APDU" ) {
                                            REQUIRE( outstation.pollAPDU() );
                                            auto the_apdu(outstation.getAPDU());
                                            REQUIRE( the_apdu.size() == apdu.size() );
                                            REQUIRE( memcmp(apdu.data(), the_apdu.data(), apdu.size()) == 0 );
                                        }
			                            THEN( "The outstation statistics should be OK" ) {
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 3 );
				                            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                            }
                                    }
                                }
                            }
                        }
                    }
			    }
		    }
		    //TODO test that a session start request from a broadcast address is ignored
		    //TODO check invalid messages (things that should provoke error returns)
	    }
    }
}

SCENARIO( "Outstation sends an initial unsolicited response, using encryption" "[unsol]") {
	GIVEN( "An Outstation stack" ) {
		io_context ioc;
		Config default_config;
        default_config.aead_algorithm_ = (decltype(default_config.aead_algorithm_))(AEADAlgorithm::aes256_gcm__);
		Tests::DeterministicRandomNumberGenerator rng;
		Outstation outstation(ioc, 0/* association ID */, default_config, rng);

		THEN( "The Outstation will be in the INITIAL state" ) {
			REQUIRE( outstation.getState() == Outstation::initial__ );
		}

		WHEN( "the Application Layer tries to send an APDU" ) {
			unsigned char apdu_buffer[2048];
			mutable_buffer apdu(apdu_buffer, sizeof(apdu_buffer));
			rng.generate(apdu); // NOTE: we really don't care about the contents of the APDU here
			outstation.postAPDU(apdu);
			THEN( "The Outstation state will be EXPECT_SESSION_START_REQUEST" ) {
				REQUIRE( outstation.getState() == Outstation::expect_session_start_request__ );
			}
			THEN( "The Outstation will attempt to send a SessionInitiation message" ) {
				REQUIRE( outstation.pollSPDU() );
				auto spdu(outstation.getSPDU());
				REQUIRE( !outstation.pollSPDU() );
				REQUIRE( spdu.size() == 8 );
				unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                unsigned char const expected_header_bytes[] = {
                        0xc0, 0x80, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00
                    };
                REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
			}
			THEN( "The outstation statistics should be OK" ) {
				REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			}
			WHEN( "The Master receives it" ) {
				Master master(ioc, 0/* association ID */, default_config, rng);
				REQUIRE( master.getState() == Master::initial__ );
				
				auto spdu(outstation.getSPDU());
				master.postSPDU(spdu);
				THEN( "The Master should be in the EXPECT_SESSION_START_RESPONSE state" ) {
					REQUIRE( master.getState() == Master::expect_session_start_response__ );
				}
			    THEN( "The Master statistics should be OK" ) {
				    REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 1 );
				    REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 1 );
				    REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				    REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			    }
				THEN( "The Master should not present anything as an APDU" ) {
					REQUIRE( !master.pollAPDU() );
				}
				THEN( "The Master should send back a SessionStartRequest" ) {
					REQUIRE( master.pollSPDU() );
					auto spdu(master.getSPDU());
					REQUIRE( !master.pollSPDU() );
					REQUIRE( spdu.size() == 10 );
					unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                    unsigned char const expected_header_bytes[] = {
                            0xc0, 0x80, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );

                    unsigned char const expected_payload_bytes[] = {
                           0x06, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected_payload_bytes, sizeof(expected_payload_bytes)) == 0 );
				}
				//TODO test cases where the Outstation sent its RequestSessionInitation message with sequence numbers 
				//     other than 1, according to OPTION_IGNORE_OUTSTATION_SEQ_ON_REQUEST_SESSION_INITIATION
				WHEN( "The Outstation receives the Master's response" ) {
					spdu = master.getSPDU();
					outstation.postSPDU(spdu);
					THEN( "The Outstation should go to the EXPECT_SET_KEYS state" ) {
						REQUIRE( outstation.getState() == Outstation::expect_session_key_change_request__ );
					}
			        THEN( "The outstation statistics should be OK" ) {
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
					THEN( "The Outstation should not present an APDU" ) {
						REQUIRE( !outstation.pollAPDU() );
					}
					THEN( "The Outstation should present an SPDU" ) {
						REQUIRE( outstation.pollSPDU() );
					}
					THEN( "The Outstation should send back a SessionStartResponse" ) {
						REQUIRE( outstation.pollSPDU() );
						auto spdu(outstation.getSPDU());
						REQUIRE( !outstation.pollSPDU() );
						REQUIRE( spdu.size() == 13 );
						unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                        unsigned char const expected_header_bytes[] = {
                              0xc0, 0x80, 0x01, 0x03, 0x00, 0x00, 0x01, 0x00
                            };
                        REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                        unsigned char const expected_payload_bytes[] = {
                              0x04
							, 0x79, 0x28, 0x11, 0xc8
                            };
                        REQUIRE( memcmp(spdu_bytes + sizeof(expected_header_bytes), expected_payload_bytes, sizeof(expected_payload_bytes)) == 0 );
					}
                    WHEN( "The Outstation sends a SessionStartResponse" ) {
                        outstation.getSPDU();
			            THEN( "The outstation statistics should be OK" ) {
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
                    }
					WHEN( "The Master receives the SessionStartResponse" ) {
						auto spdu(outstation.getSPDU());
						master.postSPDU(spdu);
						THEN( "The Master should go to the EXPECT_SESSION_ACK state" ) {
							REQUIRE( master.getState() == SecurityLayer::expect_session_key_change_response__ );
						}
			            THEN( "The Master statistics should be OK" ) {
				            REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				            REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
				        THEN( "The Master should not present anything as an APDU" ) {
					        REQUIRE( !master.pollAPDU() );
				        }
						THEN( "The Master will send SessionKeyChangeRequest message" ) {
							auto spdu(master.getSPDU());
							REQUIRE( spdu.size() == 100 );
							unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                            unsigned char const expected_header_bytes[] = {
                                    0xc0, 0x80, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00
                                };
                            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
							unsigned char const expected[] = {
                                  0x02, 0x0b, 0x58, 0x00
                                // we don't check the rest of the payload here
                                };
							REQUIRE( memcmp(spdu_bytes + (8/*SPDU header size*/), expected, sizeof(expected)) == 0 );
						}
						//TODO check invalid messages (things that should provoke error returns)
						//TODO check with the wrong sequence number
                        WHEN( "The Outstation receives the SessionKeyChangeRequest message" ) {
                            auto spdu(master.getSPDU());
                            outstation.postSPDU(spdu);
                            THEN( "The outstation should go to the ACTIVE state" ) {
        						REQUIRE( outstation.getState() == Outstation::active__ );
                            }
					        THEN( "The Outstation should not present an APDU" ) {
						        REQUIRE( !outstation.pollAPDU() );
					        }
			                THEN( "The outstation statistics should be OK" ) {
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 3 );
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                }
                            THEN( "The Outstation will attempt to send a SessionKeyChangeResponse message" ) {
                                REQUIRE( outstation.pollSPDU() );
				                auto spdu(outstation.getSPDU());
				                REQUIRE( !outstation.pollSPDU() );
				                REQUIRE( spdu.size() == 24 );
				                unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                                unsigned char const expected_header_bytes[] = {
                                        0xc0, 0x80, 0x01, 0x05, 0x00, 0x00, 0x01, 0x00
                                    };
                                REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                                // we don't need to check the payload here
                            }
                            WHEN( "The Outstation to sends a SessionKeyChangeResponse message" ) {
                                auto spdu(outstation.getSPDU());
                                THEN( "The Outstation has prepared the authenticated-APDU SPDU" )
                                {
                                    outstation.update();
                                    REQUIRE( outstation.pollSPDU() ); // for the pending APDU
				                    auto spdu(outstation.getSPDU());
				                    REQUIRE( !outstation.pollSPDU() );
				                    REQUIRE( spdu.size() == 2048+8+2+16 );
				                    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                                    unsigned char const expected_header_bytes[] = {
                                            0xc0, 0x80, 0x01, 0x06, 0x00, 0x00, 0x01, 0x00
                                        };
                                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                                    // we only check that the APDU is different here from what was sent
                                    REQUIRE( memcmp(apdu.data(), spdu_bytes + 10, apdu.size()) != 0 );
                                }
			                    THEN( "The outstation statistics should be OK" ) {
				                    REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 3 );
				                    REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                    REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                    REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                    }
                                WHEN( "The Master receives it" ) {
                                    master.postSPDU(spdu);
                                    THEN( "The Master should go to the ACTIVE state" ) {
                                        REQUIRE( master.getState() == SecurityLayer::active__ );
                                    }
			                        THEN( "The Master statistics should be OK" ) {
				                        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				                        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 3 );
				                        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                        }
				                    THEN( "The Master should not present anything as an APDU" ) {
					                    REQUIRE( !master.pollAPDU() );
				                    }
                                    WHEN( "The Outstation canceled the APDU (timeout)" ) {
                                        outstation.onApplicationLayerTimeout();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation canceled the APDU (application reset)" ) {
                                        outstation.onApplicationReset();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation canceled the APDU (cancel)" ) {
                                        outstation.cancelPendingAPDU();
                                        THEN( "No SPDU will be pending" ) {
                                             REQUIRE( !outstation.pollSPDU() );
                                        }
                                    }
                                    WHEN( "The Outstation does send the APDU" ) {
                                        outstation.update();
                                        REQUIRE( outstation.pollSPDU() ); // for the pending APDU
				                        auto spdu(outstation.getSPDU());
				                        REQUIRE( !outstation.pollSPDU() );
			                            THEN( "The outstation statistics should be OK" ) {
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 4 );
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 1 );	
				                            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                            }
                                        WHEN( "The Master receives it" ) {
                                            master.postSPDU(spdu);
                                            THEN( "The Master will have an APDU ready for consumption" ) {
                                                REQUIRE( master.pollAPDU() );
                                            }
                                            THEN( "The Master can spit out the same APDU we originally sent" ) {
                                                auto the_apdu(master.getAPDU());
                                                REQUIRE( the_apdu.size() == apdu.size() );
                                                REQUIRE( memcmp(the_apdu.data(), apdu.data(), apdu.size()) == 0 );
                                            }
                                        }
                                    }
                                }
                            }
                        }
					}
				}
				//TODO test that a session start request from a broadcast address is ignored
				//TODO check invalid messages (things that should provoke error returns)
			}
		}
	}
}

/* We'll only check message headers byte-by-byte here to allow for slightly cleaner code */
SCENARIO( "Master sends an initial poll, using encryption" "[master-init]") {
	GIVEN( "A Master that needs to send an APDU" ) {
		io_context ioc;
		Config default_config;
        default_config.aead_algorithm_ = (decltype(default_config.aead_algorithm_))(AEADAlgorithm::aes256_gcm__);
		Tests::DeterministicRandomNumberGenerator rng;
		Master master(ioc, 0/* association ID */, default_config, rng);
		REQUIRE( master.getState() == Master::initial__ );
		unsigned char apdu_buffer[2048];
		mutable_buffer apdu(apdu_buffer, sizeof(apdu_buffer));
		rng.generate(apdu); // NOTE: we really don't care about the contents of the APDU here
				
		master.postAPDU(apdu);
		THEN( "The Master go to in the EXPECT_SESSION_START_RESPONSE state" ) {
			REQUIRE( master.getState() == Master::expect_session_start_response__ );
		}
		THEN( "The Master statistics should be OK" ) {
			REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 1 );
			REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 0 );
			REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
			REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
			REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
			REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
			static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
		}
		THEN( "The Master should not present anything as an APDU" ) {
			REQUIRE( !master.pollAPDU() );
		}
		THEN( "The Master should send a SessionStartRequest" ) {
			REQUIRE( master.pollSPDU() );
			auto spdu(master.getSPDU());
			REQUIRE( !master.pollSPDU() );
			REQUIRE( spdu.size() == 10 );
			unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
            unsigned char const expected_header_bytes[] = {
                  0xc0, 0x80, 0x01, 0x02, 0x00, 0x00, 0x01, 0x00
                };
            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
		}
        GIVEN( "A newly booted Outstation" ) {
            Outstation outstation(ioc, 0/* association ID */, default_config, rng);
            REQUIRE( outstation.getState() == Outstation::initial__ );

		    WHEN( "The Outstation receives the Master's request" ) {
			    auto spdu(master.getSPDU());
			    outstation.postSPDU(spdu);
			    THEN( "The Outstation should go to the EXPECT_SET_KEYS state" ) {
				    REQUIRE( outstation.getState() == Outstation::expect_session_key_change_request__ );
			    }
			    THEN( "The outstation statistics should be OK" ) {
				    REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				    REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				    REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				    REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			    }
			    THEN( "The Outstation should not present an APDU" ) {
				    REQUIRE( !outstation.pollAPDU() );
			    }
			    THEN( "The Outstation should present an SPDU" ) {
				    REQUIRE( outstation.pollSPDU() );
			    }
			    THEN( "The Outstation should send back a SessionStartResponse" ) {
				    REQUIRE( outstation.pollSPDU() );
				    auto spdu(outstation.getSPDU());
				    REQUIRE( !outstation.pollSPDU() );
				    REQUIRE( spdu.size() == 13 );
				    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                    unsigned char const expected_header_bytes[] = {
                          0xc0, 0x80, 0x01, 0x03, 0x00, 0x00, 0x01, 0x00
                        };
                    REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
			    }
                WHEN( "The Outstation sends a SessionStartResponse" ) {
                    outstation.getSPDU();
			        THEN( "The outstation statistics should be OK" ) {
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
                }
			    WHEN( "The Master receives the SessionStartResponse" ) {
				    auto spdu(outstation.getSPDU());
				    master.postSPDU(spdu);
				    THEN( "The Master should go to the EXPECT_SESSION_ACK state" ) {
					    REQUIRE( master.getState() == SecurityLayer::expect_session_key_change_response__ );
				    }
			        THEN( "The Master statistics should be OK" ) {
				        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 1 );
				        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			        }
				    THEN( "The Master should not present anything as an APDU" ) {
					    REQUIRE( !master.pollAPDU() );
				    }
				    THEN( "The Master will send SessionKeyChangeRequest message" ) {
					    auto spdu(master.getSPDU());
					    REQUIRE( spdu.size() == 100 );
					    unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                        unsigned char const expected_header_bytes[] = {
                                0xc0, 0x80, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00
                            };
                        REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
				    }
				    //TODO check invalid messages (things that should provoke error returns)
				    //TODO check with the wrong sequence number
                    WHEN( "The Outstation receives the SessionKeyChangeRequest message" ) {
                        auto spdu(master.getSPDU());
                        outstation.postSPDU(spdu);
                        THEN( "The outstation should go to the ACTIVE state" ) {
        				    REQUIRE( outstation.getState() == Outstation::active__ );
                        }
					    THEN( "The Outstation should not present an APDU" ) {
						    REQUIRE( !outstation.pollAPDU() );
					    }
			            THEN( "The outstation statistics should be OK" ) {
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			            }
                        THEN( "The Outstation will attempt to send a SessionKeyChangeResponse message" ) {
                            REQUIRE( outstation.pollSPDU() );
				            auto spdu(outstation.getSPDU());
				            REQUIRE( !outstation.pollSPDU() );
				            REQUIRE( spdu.size() == 24 );
				            unsigned char const *spdu_bytes(static_cast< unsigned char const * >(spdu.data()));
                            unsigned char const expected_header_bytes[] = {
                                  0xc0, 0x80, 0x01, 0x05, 0x00, 0x00, 0x01, 0x00
                                };
                            REQUIRE( memcmp(spdu_bytes, expected_header_bytes, sizeof(expected_header_bytes)) == 0 );
                        }
                        WHEN( "The Outstation to sends a SessionKeyChangeResponse message" ) {
                            auto spdu(outstation.getSPDU());
			                THEN( "The outstation statistics should be OK" ) {
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 2 );
				                REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                }
                            WHEN( "The Master receives it" ) {
                                master.postSPDU(spdu);
                                THEN( "The Master should go to the ACTIVE state" ) {
                                    REQUIRE( master.getState() == SecurityLayer::active__ );
                                }
			                    THEN( "The Master statistics should be OK" ) {
				                    REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 2 );
				                    REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				                    REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                    REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                    static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                    }
				                THEN( "The Master should not present anything as an APDU" ) {
					                REQUIRE( !master.pollAPDU() );
				                }
                                THEN( "The Master is ready to send the APDU" ) {
                                    master.update();
                                    REQUIRE( master.pollSPDU() );
                                }
                                WHEN( "the Master sends the APDU" ) {
                                    master.update();
                                    REQUIRE( master.pollSPDU() );
                                    spdu = master.getSPDU();
                                    REQUIRE( !master.pollSPDU() );
			                        THEN( "The Master statistics should be OK" ) {
				                        REQUIRE( master.getStatistic(Statistics::total_messages_sent__) == 3 );
				                        REQUIRE( master.getStatistic(Statistics::total_messages_received__) == 2 );
				                        REQUIRE( master.getStatistic(Statistics::discarded_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::error_messages_sent__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::unexpected_messages__) == 0 );
				                        REQUIRE( master.getStatistic(Statistics::secure_messages_sent_) == 1 );	
				                        static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                        }
                                    WHEN( "the outstation receives it" ) {
                                        outstation.postSPDU(spdu);
                                        THEN( "the Outstation will present the APDU" ) {
                                            REQUIRE( outstation.pollAPDU() );
                                            auto the_apdu(outstation.getAPDU());
                                            REQUIRE( the_apdu.size() == apdu.size() );
                                            REQUIRE( memcmp(apdu.data(), the_apdu.data(), apdu.size()) == 0 );
                                        }
			                            THEN( "The outstation statistics should be OK" ) {
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_sent__) == 2 );
				                            REQUIRE( outstation.getStatistic(Statistics::total_messages_received__) == 3 );
				                            REQUIRE( outstation.getStatistic(Statistics::discarded_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::error_messages_sent__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::unexpected_messages__) == 0 );
				                            REQUIRE( outstation.getStatistic(Statistics::secure_messages_sent_) == 0 );	
				                            static_assert(static_cast< int >(Statistics::statistics_count__) == 6, "New statistic added?");
			                            }
                                    }
                                }
                            }
                        }
                    }
			    }
		    }
		    //TODO test that a session start request from a broadcast address is ignored
		    //TODO check invalid messages (things that should provoke error returns)
	    }
    }
}
