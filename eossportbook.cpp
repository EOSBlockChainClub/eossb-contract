#include <eosiolib/eosio.hpp>
#include <eosiolib/serialize.hpp>

class eossportbook : public eosio::contract {
public:
    eossportbook(account_name self) : eosio::contract(self){}

    // @abi table offer i64
    struct offer {
        uint64_t runner_id;
        account_name originator;
        double_t price;
        double_t amount;

        uint64_t primary_key() const { return runner_id; }
        account_name by_originator() const { return originator; }

        EOSLIB_SERIALIZE(offer, (runner_id)(originator)(price)(amount))
    };

    typedef eosio::multi_index<N(offer), offer
      indexed_by<N(offerorig), eosio::const_mem_fun<offer, account_name, &offer::by_originator> >
    > offer_index;
    
    // @abi table offer i64
    struct offerlock {
        uint64_t runner_id;
        
        uint64_t primary_key() const { return runner_id; }

        EOSLIB_SERIALIZE(offerlock, (runner_id))
    };

    typedef eosio::multi_index<N(offerlock), offerlock> offerlock_index;

    // @abi table offer i64
    struct bet {
        uint64_t runner_id;
        account_name originator;
        account_name acceptor;
        uint64_t mb_offer_id;
        double_t price;
        double_t amount;

        uint64_t primary_key() const { return runner_id; }
        uint64_t by_mb_offer() const { return mb_offer_id; }

        EOSLIB_SERIALIZE(bet, (runner_id)(originator)(acceptor)(mb_offer_id)(price)(amount))
    };

    typedef eosio::multi_index<N(bet), bet
      indexed_by<N(mboffer), eosio::const_mem_fun<offer, uint64_t, &bet::by_mb_offer> >
    > bet_index;
    
    // @abi action
    void submitoffer(account_name from, offer t_offer) {
      // transaction have to be signed by user submitic offer
      require_auth(from);
      
      // get offerlock table stored in user's scope
      offerlock_index offerlocks(_self, from);
      
      // check is user have already pending transaction on this runner
      auto itr = offerlocks.find(t_offer.runner_id);
      eosio_assert(itr == offerlocks.end(), "Offer can be submited. There is already pending offer on this runner");
      
      // set temporary lock on offers on this runner
      offerlocks.emplace(_self, [&](auto &ofl) {
          ofl.runner_id = t_offer.runner_id;
      });
      
      // deposit funds from user
      currency::inline_transfer( from, _this_contract, quantity, "offer" );
      
      // add new offer to pending offers
      offer_index offers(_self, _self);
      offers.emplace(_self, [&](auto &of) {
          of.runner_id = t_offer.runner_id;
          of.price = t_offer.price;
          of.amount = t_offer.amount
          of.originator = t_offer.from;
      });
    }
    
    // @abi action
    void acceptoffer(account_name orginator, uint64_t runner_id, double_t amount, uint64_t mb_offer_id, account_name acceptor) {
      // transaction have to be signed by house wallet
      require_auth(_self);
      
      offer_index offers(_self, _self);
      auto origin_index = offers.get_index<N( offerorig )>();
      
      auto itr = origin_index.find(orgin);
      while(itr!=origin_index.end){
        offer of = *itr;
        if(of.runner_id==runner_id){
          
          // create acceptor bet
          bet_index bets(_self, originator);
          bets.emplace( _self /*payer*/, [&]( auto& bt ) {
            bt.runner_id = of.runner_id;
            bt.originator = of.originator;
            bt.acceptor = acceptor;
            bt.mb_offer_id = mb_offer_id;
            bt.price = of.price;
            bt.amount = of.amount;
         } );
         
         currency::inline_transfer( acceptor, _this_contract, quantity, "accepted" );
          
          // remove offer
          origin_index.erase(itr)
        }
      }
    }
    
    // @abi action
    void payout(uint64_t mb_offer_id, bool originator_win) {
      bet_index bets(_self, originator);
      auto mb_offers = bets.get_index<N(mboffer)>();
      
      auto itr = mb_offers.find(mb_offer_id);
      eosio_assert(itr == offerlocks.end(), "Bet not found");
      
      bet b = *itr;
      
      if(originator_win){
        currency::inline_transfer( _self, b.originator, quantity, "win" );
      }else{
        currency::inline_transfer( _self, b.acceptor, quantity, "win" );
      }
      
      mb_offers.erase(itr);
    }

};

EOSIO_ABI(eossportbook, (submitoffer)(acceptoffer)(payout))
