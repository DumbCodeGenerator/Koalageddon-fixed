#include "pch.h"
#include "uplay_r2_hooks.h"
#include "platforms/uplay_r2/UplayR2.h"

#define GET_ORIGINAL_FUNC(FUNC) \
	static auto proxyFunc = PLH::FnCast(BasePlatform::trampolineMap[#FUNC], FUNC);

using namespace UPC;

vector<Product> products;

vector<unsigned int> dlcs;
vector<unsigned int> items;

string productTypeToString(ProductType type)
{
	switch(type)
	{
		case ProductType::App:
			return "App";
		case ProductType::DLC:
			return "DLC";
		case ProductType::Item:
			return "Item";
		default:
			return "Unexpected Type";
	}
}

int UPC_Init(unsigned int version, unsigned int appID)
{
	logger->info("{} -> version: {}, appid: {}", __func__, version, appID);

	products.push_back(Product(appID, ProductType::App));
	for(auto& dlc : dlcs)
	{
		products.push_back(Product(dlc, ProductType::DLC));
	}

	for(auto& item : items)
	{
		products.push_back(Product(item, ProductType::Item));
	}

	GET_ORIGINAL_FUNC(UPC_Init);

	return proxyFunc(version, appID);
}

int UPC_ProductListFree(void* context, ProductList* inProductList)
{
	logger->debug(__func__);
	if(inProductList)
	{
		for(unsigned i = 0; i < inProductList->length; ++i)
		{
			delete inProductList->data[i];
		}

		delete[] inProductList->data;
	}

	delete inProductList;
	return 0;
}

void ProductListGetCallback(unsigned long arg1, void* data)
{
    // Log arg1 and data as unsigned long and pointer (cast to uintptr_t for logging)
    logger->debug("arg1: {}, data: {}", arg1, reinterpret_cast<uintptr_t>(data));

    auto callbackContainer = (UPC::CallbackContainer*)data;

    logger->debug("Legit product list:");

    vector<UPC::Product*> missingProducts;
    auto list = callbackContainer->legitProductList;
    for (uint32_t i = 0; i < list->length; i++)
    {
        auto product = list->data[i];

        // Convert ProductType to string manually
        std::string productTypeStr;
        switch (product->type) {
        case UPC::ProductType::App:
            productTypeStr = "App";
            break;
        case UPC::ProductType::DLC:
            productTypeStr = "DLC";
            break;
        case UPC::ProductType::Item:
            productTypeStr = "Item";
            break;
        default:
            productTypeStr = "Unknown";
            break;
        }

        // Log each product's details with the manual string conversion
        logger->debug(
            "\tApp ID: {}, Type: {}, Mystery1: {}, Mystery2: {}, Always0: {}, Always3: {}",
            product->appid, productTypeStr, product->mystery1, product->mystery2, product->always_0, product->always_3
        );

        if (!(vectorContains(dlcs, product->appid) || vectorContains(items, product->appid)))
            if (product->type != UPC::ProductType::App)
                missingProducts.push_back(product);
    }

    if (!missingProducts.empty())
        logger->warn("Some of the legitimately owned products are missing from the config: ");

    for (const auto& missingProduct : missingProducts)
    {
        // Log the product type string directly
        std::string productTypeStr;
        switch (missingProduct->type) {
        case UPC::ProductType::App:
            productTypeStr = "App";
            break;
        case UPC::ProductType::DLC:
            productTypeStr = "DLC";
            break;
        case UPC::ProductType::Item:
            productTypeStr = "Item";
            break;
        default:
            productTypeStr = "Unknown";
            break;
        }
        logger->warn("\tApp ID: {}, Type: {}", missingProduct->appid, productTypeStr);
    }

    // Free the legit product list
    GET_ORIGINAL_FUNC(UPC_ProductListFree);
    proxyFunc(callbackContainer->context, callbackContainer->legitProductList);

    callbackContainer->originalCallback(arg1, callbackContainer->callbackData);
    logger->debug("Game callback was called");

    delete callbackContainer;
}




int UPC_ProductListGet(
	void* context,
	char* inOptUserIdUtf8,
	unsigned int filter,
	UPC::ProductList** outProductList,
	UPC::UplayCallback callback,
	void* callbackData
)
{
	logger->debug("{}", __func__);

	auto productList = new ProductList();
	productList->data = new Product * [products.size()];
	for(unsigned int i = 0; i < products.size(); i++)
	{
		productList->data[i] = new Product(products.at(i));
	}

	productList->length = (uint32_t) products.size();
	*outProductList = productList;

	auto callbackContainer = new CallbackContainer{
		context,
		callback,
		callbackData
	};

	GET_ORIGINAL_FUNC(UPC_ProductListGet);
	return proxyFunc(context, inOptUserIdUtf8, filter, &callbackContainer->legitProductList, ProductListGetCallback, callbackContainer);
}

