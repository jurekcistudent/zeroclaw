# Railway Deployment Guide for ZeroClaw

This guide contains the steps to deploy the modified ZeroClaw project to Railway, including the Google Chrome AI search integration.

## 1. Prerequisites

- A GitHub repository containing your tailored ZeroClaw codebase.
- A [Railway app](https://railway.app) account.
- Your Google Custom Search Engine ID and API Key (for the Chrome AI search integration).

## 2. Deploying on Railway

1. **Create a New Project on Railway:**
   - Log into Railway and select **New Project** > **Deploy from GitHub repo**.
   - Select your ZeroClaw repository.

2. **Wait for Initial Build:**
   - Railway will automatically detect the `railway.toml` and standard `Dockerfile`.
   - The initial build will compile the Rust binary (this may take 5-10 minutes).

3. **Configure Environment Variables:**
   - Go to the **Variables** tab of your Railway service.
   - Add the required secrets for your runtime config. ZeroClaw generates a default `config.toml` that respects environment or explicit configs. If you are leveraging environment substitution or modifying `config.toml` at runtime, set your tokens accordingly.
   - **Recommended Variables to manually configure within `config.toml` inside a Railway volume or injected config script:**
     - `google_api_key`: Your Google API key for custom search.
     - `google_cse_id`: Your Google Custom Search Engine ID.
   - Note: Railway automatically injects the `PORT` variable, and our `Dockerfile`/`railway.toml` bindings use this so ZeroClaw gateway starts dynamically on Railway's routing port.

4. **Verify Deployment:**
   - Go to the **Settings** tab and under **Networking**, click **Generate Domain** to expose the API.
   - Access the domain URL. You should see the ZeroClaw gateway endpoints responding.

## 3. Chrome AI Search Verification

- Send a query to the agent triggering the web search tool.
- Check Railway logs to ensure the tool invokes `https://www.googleapis.com/customsearch/v1` successfully and returns the desired AI search results.

## Troubleshooting

- **Build Failing (OOM):** Rust builds can be memory-intensive. Ensure your Railway service tier has sufficient RAM (min 2GB ideally for the build step).
- **Port Binding Issues:** ZeroClaw defaults to standard bindings. If it fails to bind, ensure the `allow_public_bind = true` is present in the `[gateway]` section in the generated `config.toml` (already applied).
