#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <deque>
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <numeric>
#include <limits>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <tuple>
#include <format>
#include <list>

// Fixed enum names with proper capitalization
enum class OrderType
{
    GoodTillCancel,
    FillAndKill
};

enum class Side
{
    Buy,
    Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

// Fixed class name capitalization
class OrderbookLevelInfos
{
public:
    OrderbookLevelInfos(const LevelInfos &bids, const LevelInfos &asks)
        : bids_(bids), asks_(asks) {}

    const LevelInfos &GetBids() const { return bids_; }
    const LevelInfos &GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order
{
public:
    // Fixed constructor name and parameter types
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_(orderType), orderId_(orderId), side_(side), price_(price),
          initialQuantity_(quantity), remainingQuantity_(quantity) {}

    // Fixed all getter method names and consistency
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    OrderType GetOrderType() const { return orderType_; }

    // Removed duplicate method
    Quantity GetFilledQuantity() const
    {
        return GetInitialQuantity() - GetRemainingQuantity();
    }

    bool IsFilled() const
    {
        return GetRemainingQuantity() == 0;
    }

    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
        {
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity", GetOrderId()));
        }
        remainingQuantity_ -= quantity;
    }

private:
    OrderId orderId_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;   // Fixed variable name
    Quantity remainingQuantity_; // Fixed variable name
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_(orderId), side_(side), price_(price), quantity_(quantity) {}

    OrderId GetOrderId() const { return orderId_; }
    Price GetPrice() const { return price_; }
    Side GetSide() const { return side_; }
    Quantity GetQuantity() const { return quantity_; } // Fixed typo

    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade(const TradeInfo &bidTrade, const TradeInfo &askTrade) // Fixed parameter name
        : bidTrade_(bidTrade), askTrade_(askTrade)
    {
    }

    const TradeInfo &GetBidTrade() const { return bidTrade_; }
    const TradeInfo &GetAskTrade() const { return askTrade_; }

private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
}; // Added missing semicolon

using Trades = std::vector<Trade>;

class Orderbook
{
private:
    struct OrderEntry // Fixed struct name capitalization
    {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_; // Fixed template parameters
    std::unordered_map<OrderId, OrderEntry> orders_;

    bool CanMatch(Side side, Price price) const
    {
        if (side == Side::Buy)
        {
            if (asks_.empty())
                return false;
            const auto &[bestAsk, _] = *asks_.begin();
            return price >= bestAsk;
        }
        else
        {
            if (bids_.empty())
                return false;
            const auto &[bestBid, _] = *bids_.begin();
            return price <= bestBid;
        }
    }

    Trades MatchOrders()
    {
        Trades trades;
        trades.reserve(orders_.size()); // Fixed typo

        while (true)
        {
            if (bids_.empty() || asks_.empty())
                break;

            auto &[bidPrice, bids] = *bids_.begin();
            auto &[askPrice, asks] = *asks_.begin();

            if (bidPrice < askPrice)
                break;

            while (bids.size() && asks.size()) // Fixed syntax
            {
                auto &bid = bids.front();
                auto &ask = asks.front();

                Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity()); // Fixed method calls

                bid->Fill(quantity);
                ask->Fill(quantity);

                trades.push_back(Trade{
                    TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity}, // Fixed method calls and variable name
                    TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});

                if (bid->IsFilled())
                {
                    bids.pop_front();
                    orders_.erase(bid->GetOrderId()); // Fixed variable name
                }
                if (ask->IsFilled())
                {
                    asks.pop_front();
                    orders_.erase(ask->GetOrderId());
                }
            }

            if (bids.empty())
                bids_.erase(bidPrice);
            if (asks.empty())
                asks_.erase(askPrice);
        }

        // Handle Fill and Kill orders that couldn't be completely filled
        if (!bids_.empty())
        {
            auto &[_, bids] = *bids_.begin();
            if (!bids.empty())
            {
                auto &order = bids.front();
                if (order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }
        }
        if (!asks_.empty())
        {
            auto &[_, asks] = *asks_.begin();
            if (!asks.empty())
            {
                auto &order = asks.front();
                if (order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }
        }

        return trades;
    }

public:
    Trades AddOrder(OrderPointer order)
    {
        if (orders_.contains(order->GetOrderId()))
            return {};

        if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
            return {};

        OrderPointers::iterator iterator;

        if (order->GetSide() == Side::Buy)
        {
            auto &orders = bids_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }
        else
        {
            auto &orders = asks_[order->GetPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin(), orders.size() - 1);
        }

        orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});
        return MatchOrders();
    }

    void CancelOrder(OrderId orderId)
    {
        if (!orders_.contains(orderId))
            return;

        const auto &[order, iterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if (order->GetSide() == Side::Sell)
        {
            auto price = order->GetPrice();
            auto &orders = asks_.at(price);
            orders.erase(iterator);
            if (orders.empty())
                asks_.erase(price);
        }
        else
        {
            auto price = order->GetPrice();
            auto &orders = bids_.at(price);
            orders.erase(iterator);
            if (orders.empty())
                bids_.erase(price);
        }
    }

    Trades ModifyOrder(OrderModify order) // Fixed method name
    {
        if (!orders_.contains(order.GetOrderId()))
            return {};

        const auto &[existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }

    std::size_t Size() const { return orders_.size(); }

    OrderbookLevelInfos GetOrderInfos() const
    {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos = [](Price price, const OrderPointers &orders)
        {
            return LevelInfo{price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                                                    [](Quantity runningSum, const OrderPointer &order) // Fixed type
                                                    {
                                                        return runningSum + order->GetRemainingQuantity();
                                                    })};
        };

        for (const auto &[price, orders] : bids_)
            bidInfos.push_back(CreateLevelInfos(price, orders));

        for (const auto &[price, orders] : asks_)
            askInfos.push_back(CreateLevelInfos(price, orders));

        return OrderbookLevelInfos{bidInfos, askInfos};
    }
};

int main()
{
    Orderbook orderbook;
    const OrderId orderId = 1;

       std::cout << "=== Order Book Demo ===" << std::endl;

    // Add a buy order
    orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    std::cout << "Orders after adding buy order: " << orderbook.Size() << std::endl; // 1

    // Cancel the order
    orderbook.CancelOrder(orderId);
    std::cout << "Orders after canceling: " << orderbook.Size() << std::endl; // 0

    // Test order matching
    std::cout << "\n=== Testing Order Matching ===" << std::endl;

    // Add buy order at 100
    auto trades1 = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 2, Side::Buy, 100, 5));
    std::cout << "Added buy order, trades executed: " << trades1.size() << std::endl;

    // Add sell order at 100 (should match)
    auto trades2 = orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 3, Side::Sell, 100, 3));
    std::cout << "Added sell order, trades executed: " << trades2.size() << std::endl;
    std::cout << "Remaining orders in book: " << orderbook.Size() << std::endl; // 1 (partial fill)

    // Test Fill and Kill order
    std::cout << "\n=== Testing Fill and Kill ===" << std::endl;
    auto trades3 = orderbook.AddOrder(std::make_shared<Order>(OrderType::FillAndKill, 4, Side::Sell, 90, 10));
    std::cout << "FAK order trades executed: " << trades3.size() << std::endl;
    std::cout << "Orders after FAK: " << orderbook.Size() << std::endl;

    return 0;
}